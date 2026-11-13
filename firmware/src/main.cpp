#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <Wire.h>
#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <BH1750.h>
#include <LiquidCrystal_I2C.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "hardware_config.h"
#include "ControlCore.h"
#ifndef SMARTGROW_LOCAL_TEST
#define SMARTGROW_LOCAL_TEST 0
#endif
#if SMARTGROW_LOCAL_TEST
#include "device_config.example.h"
#elif __has_include("device_config.h")
#include "device_config.h"
#else
#include "device_config.example.h"
#endif
#include "portal.h"
#if SMARTGROW_LOCAL_TEST
#include "local_test_page.h"
#endif

constexpr bool LOCAL_TEST = SMARTGROW_LOCAL_TEST == 1;
constexpr char VERSION[] = "0.1.1";
constexpr uint32_t WIFI_RETRY_MS = 20000, AP_AFTER_MS = 60000, WIFI_TRIAL_MS = 60000;
DHT dht(DHT_PIN, DHT11);
OneWire oneWire(PROBE_PIN);
DallasTemperature probe(&oneWire);
BH1750 light;
LiquidCrystal_I2C lcd(LCD_ADDRESS, 16, 2);
Preferences prefs;
WebServer portal(80);
DNSServer dns;
WiFiClientSecure secureClient;
PubSubClient mqtt(secureClient);
TimedRelay pump, lamp;
portMUX_TYPE relayMux = portMUX_INITIALIZER_UNLOCKED;
String baseTopic, bootId, csrfToken, knownSsid, knownPassword;
String candidateSsid, candidatePassword, wifiCommandId, pendingResultId, pendingResultStatus;
bool portalOn = false, lightReady = false, lcdReady = false, trial = false, trialRemote = false;
bool waitingToStartTrial = false, wasConnected = false;
uint32_t lastWifiTry = 0, wifiLostAt = 0, trialStarted = 0, trialStartAt = 0;
uint32_t lastTelemetry = 0, lastMqttTry = 0, lastSensor = 0, lastLcd = 0, probeRequested = 0;
bool probeWaiting = false, haveProbe = false;
DeviceAddress probeAddress;
float airC = NAN, humidity = NAN, probeC = NAN, lux = NAN, soilPercent = NAN;
int soilRaw = 0;
String seenIds[32];
unsigned seenIndex = 0;
String localId, localApPassword, localWifiResult = "idle";
uint32_t lastSerialReport = 0;
bool soilReady = false;

String deviceId() { return LOCAL_TEST ? localId : String(DEVICE_ID); }
String apPassword() { return LOCAL_TEST ? localApPassword : String(SETUP_AP_PASSWORD); }
String apName() { return String(LOCAL_TEST ? "SmartGrow-Test-" : "SmartGrow-Setup-") + deviceId(); }

String randomHex() {
  char value[33];
  snprintf(value, sizeof(value), "%08lx%08lx%08lx%08lx", (unsigned long)esp_random(),
           (unsigned long)esp_random(), (unsigned long)esp_random(), (unsigned long)esp_random());
  return String(value);
}
bool configured() {
  if (LOCAL_TEST) return localId.length() && localApPassword.length() >= 8;
  return String(DEVICE_ID) != "UNPROVISIONED" && strlen(DEVICE_ID) <= 16 && strlen(MQTT_ROOT_CA) > 100 &&
         strlen(SETUP_AP_PASSWORD) >= 8 && strlen(SETUP_AP_PASSWORD) <= 63;
}
void outputsOff() {
  portENTER_CRITICAL(&relayMux);
  pump.on = false; lamp.on = false;
  portEXIT_CRITICAL(&relayMux);
}
void relayTask(void*) {
  // Independent of TLS, DNS, HTTP and sensor reads: deadlines keep running.
  while (true) {
    portENTER_CRITICAL(&relayMux);
    pump.tick(millis()); lamp.tick(millis());
    const bool p = pump.on, l = lamp.on;
    portEXIT_CRITICAL(&relayMux);
    digitalWrite(PUMP_PIN, (p != PUMP_ACTIVE_LOW) ? HIGH : LOW);
    digitalWrite(LAMP_PIN, (l != LAMP_ACTIVE_LOW) ? HIGH : LOW);
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
bool publishDoc(const String& topic, const JsonDocument& doc, bool retained = false) {
  if (!mqtt.connected()) return false;
  char buffer[1536];
  size_t n = serializeJson(doc, buffer, sizeof(buffer));
  if (n >= sizeof(buffer)) return false;
  return mqtt.publish(topic.c_str(), reinterpret_cast<const uint8_t*>(buffer), n, retained);
}
void ack(const String& id, const char* status, const char* detail) {
  StaticJsonDocument<256> doc;
  doc["id"] = id; doc["status"] = status; doc["detail"] = detail;
  publishDoc(baseTopic + "/ack", doc);
}
void floatValue(JsonDocument& doc, const char* key, float value) {
  if (isfinite(value)) doc[key] = value; else doc[key] = nullptr;
}
void telemetry() {
  StaticJsonDocument<768> doc;
  doc["boot_id"] = bootId; doc["firmware"] = VERSION;
  if (soilReady) doc["soil_raw"] = soilRaw; else doc["soil_raw"] = nullptr;
  floatValue(doc, "soil_pct", soilPercent); floatValue(doc, "air_c", airC);
  floatValue(doc, "humidity_pct", humidity); floatValue(doc, "probe_c", probeC); floatValue(doc, "lux", lux);
  doc["rssi"] = WiFi.RSSI();
  portENTER_CRITICAL(&relayMux);
  bool p = pump.on, l = lamp.on;
  portEXIT_CRITICAL(&relayMux);
  doc["pump"] = p; doc["lamp"] = l;
  doc["pump_enabled"] = ENABLE_PUMP; doc["lamp_enabled"] = ENABLE_LAMP;
  publishDoc(baseTopic + "/telemetry", doc);
  lastTelemetry = millis();
}
bool validWifi(const String& ssid, const String& password) {
  return ssid.length() >= 1 && ssid.length() <= 32 && password.length() >= 8 && password.length() <= 63;
}
void scheduleWifi(const String& ssid, const String& password, bool remote, const String& id) {
  candidateSsid = ssid; candidatePassword = password; trialRemote = remote; wifiCommandId = id;
  waitingToStartTrial = true; trialStartAt = millis() + 600;
  if (LOCAL_TEST) localWifiResult = "trying";
}
void onCommand(char* topic, byte* payload, unsigned int length) {
  if (String(topic) != baseTopic + "/command" || length > 1536) return;
  StaticJsonDocument<1536> doc;
  if (deserializeJson(doc, payload, length)) return;
  String id = doc["id"] | "";
  if (id.length() != 36) return;
  if (String(doc["boot_id"] | "") != bootId) { ack(id, "rejected", "wrong_boot"); return; }
  if (!commandFresh(time(nullptr), doc["issued_at"].as<int64_t>(), doc["expires_at"].as<int64_t>())) {
    ack(id, "rejected", "expired"); return;
  }
  for (const String& seen : seenIds) if (seen == id) return; // Never extend a timer on MQTT duplicates.
  seenIds[seenIndex++ % 32] = id;
  String kind = doc["kind"] | "";
  if (kind == "relay") {
    String target = doc["target"] | "";
    if ((target != "pump" && target != "lamp") || !doc["on"].is<bool>() || !doc["duration_s"].is<unsigned>()) {
      ack(id, "rejected", "invalid_command"); return;
    }
    bool requested = doc["on"].as<bool>();
    bool enabled = target == "pump" ? ENABLE_PUMP : ENABLE_LAMP;
    unsigned seconds = doc["duration_s"];
    unsigned maximum = target == "pump" ? PUMP_MAX_SECONDS : LAMP_MAX_SECONDS;
    if (requested && (!enabled || seconds < 1 || seconds > maximum)) {
      ack(id, "rejected", enabled ? "invalid_command" : "outputs_locked"); return;
    }
    portENTER_CRITICAL(&relayMux);
    (target == "pump" ? pump : lamp).set(requested, seconds, maximum, enabled, millis());
    portEXIT_CRITICAL(&relayMux);
    ack(id, "applied", "relay_set"); telemetry();
  } else if (kind == "wifi") {
    if (trial || waitingToStartTrial) { ack(id, "rejected", "busy"); return; }
    String ssid = doc["ssid"] | "", password = doc["password"] | "";
    if (!validWifi(ssid, password)) { ack(id, "rejected", "invalid_command"); return; }
    ack(id, "accepted", "wifi_testing");
    scheduleWifi(ssid, password, true, id);
  } else ack(id, "rejected", "invalid_command");
}
bool localPortalClient() {
  if (!portalOn) return false;
  IPAddress remote = portal.client().remoteIP(), local = WiFi.softAPIP();
  return remote[0] == local[0] && remote[1] == local[1] && remote[2] == local[2];
}
void page() {
  if (!localPortalClient()) { portal.send(403, "text/plain", "Hubungkan HP ke hotspot pengaturan alat."); return; }
#if SMARTGROW_LOCAL_TEST
  String html = FPSTR(LOCAL_TEST_HTML);
#else
  String html = FPSTR(PORTAL_HTML);
#endif
  html.replace("%TOKEN%", csrfToken); html.replace("%DEVICE%", deviceId());
  portal.sendHeader("Cache-Control", "no-store");
  portal.sendHeader("X-Frame-Options", "DENY");
  portal.send(200, "text/html; charset=utf-8", html);
}
#if SMARTGROW_LOCAL_TEST
void localData() {
  if (!localPortalClient()) { portal.send(403); return; }
  StaticJsonDocument<1024> doc;
  doc["device"] = deviceId(); doc["uptime_s"] = millis() / 1000;
  doc["firmware"] = VERSION;
  if (soilReady) doc["soil_raw"] = soilRaw; else doc["soil_raw"] = nullptr;
  floatValue(doc, "soil_pct", soilPercent); floatValue(doc, "air_c", airC);
  floatValue(doc, "humidity_pct", humidity); floatValue(doc, "probe_c", probeC); floatValue(doc, "lux", lux);
  doc["soil_calibrated"] = SOIL_CALIBRATED;
  doc["lcd_found"] = lcdReady; doc["bh1750_found"] = lightReady; doc["probe_found"] = haveProbe;
  doc["wifi_connected"] = WiFi.status() == WL_CONNECTED;
  doc["wifi_phase"] = localWifiResult;
  doc["wifi_ssid"] = WiFi.status() == WL_CONNECTED ? WiFi.SSID() : "";
  doc["wifi_ip"] = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "";
  doc["pump_enabled"] = ENABLE_PUMP; doc["lamp_enabled"] = ENABLE_LAMP;
  portENTER_CRITICAL(&relayMux);
  bool p = pump.on, l = lamp.on;
  portEXIT_CRITICAL(&relayMux);
  doc["pump"] = p; doc["lamp"] = l;
  String out; serializeJson(doc, out);
  portal.sendHeader("Cache-Control", "no-store"); portal.send(200, "application/json", out);
}
void localRelay() {
  if (!localPortalClient() || portal.arg("token") != csrfToken) { portal.send(403, "text/plain", "Akses ditolak. Muat ulang halaman."); return; }
  String target = portal.arg("target"), action = portal.arg("action");
  if ((target != "pump" && target != "lamp") || (action != "on" && action != "off")) {
    portal.send(400, "text/plain", "Perintah tidak valid."); return;
  }
  if (action == "on" && (trial || waitingToStartTrial)) { portal.send(409, "text/plain", "Tunggu percobaan Wi-Fi selesai."); return; }
  bool enabled = target == "pump" ? ENABLE_PUMP : ENABLE_LAMP;
  portENTER_CRITICAL(&relayMux);
  bool ok = (target == "pump" ? pump : lamp).set(action == "on", 2, 5, enabled, millis());
  portEXIT_CRITICAL(&relayMux);
  portal.send(ok ? 200 : 409, "text/plain", ok ? "Perintah diterapkan. Tes ON berhenti otomatis setelah 2 detik." : "Relay masih terkunci di hardware_config.h.");
}
#endif
void configurePortalRoutes() {
  portal.on("/", HTTP_GET, page);
#if SMARTGROW_LOCAL_TEST
  portal.on("/data", HTTP_GET, localData);
  portal.on("/relay", HTTP_POST, localRelay);
#endif
  portal.on("/save", HTTP_POST, []() {
    if (!localPortalClient() || portal.arg("token") != csrfToken) { portal.send(403, "text/plain", "Akses ditolak."); return; }
    String ssid = portal.arg("ssid"), password = portal.arg("password");
    if (!validWifi(ssid, password)) { portal.send(400, "text/plain", "SSID/kata sandi tidak valid."); return; }
    if (trial || waitingToStartTrial) { portal.send(409, "text/plain", "Masih mencoba jaringan. Tunggu satu menit."); return; }
    if (LOCAL_TEST) portal.send(202, "text/plain; charset=utf-8", "Mencoba Wi-Fi hingga 60 detik. Hotspot uji tetap tersedia; sambungkan ulang jika HP terputus.");
    else portal.send(200, "text/html; charset=utf-8", "<meta name='viewport' content='width=device-width'><h2>SmartGrow sedang menghubungkan...</h2><p>Jika berhasil, hotspot akan ditutup. Jika gagal, sambungkan kembali ke hotspot alat dan coba lagi.</p>");
    scheduleWifi(ssid, password, false, "");
  });
  portal.onNotFound([]() {
    if (!localPortalClient()) { portal.send(403); return; }
    portal.sendHeader("Location", "http://192.168.4.1/", true); portal.send(302, "text/plain", "");
  });
}
void openPortal() {
  if (portalOn || !configured()) return;
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));
  if (!WiFi.softAP(apName().c_str(), apPassword().c_str())) return;
  csrfToken = randomHex(); portalOn = true;
  dns.start(53, "*", IPAddress(192,168,4,1)); portal.begin();
}
void closePortal() {
  if (LOCAL_TEST) return; // Keep the test page accessible even without a router.
  if (!portalOn) return;
  portal.stop(); dns.stop(); WiFi.softAPdisconnect(true); portalOn = false;
  WiFi.mode(WIFI_STA);
}
bool rememberWifi() {
  StaticJsonDocument<256> doc;
  doc["ssid"] = candidateSsid; doc["password"] = candidatePassword;
  String out; serializeJson(doc, out);
  // One NVS entry makes the accepted pair atomic. Never overwrite with an untested candidate.
  if (prefs.putString("wifi", out) != out.length()) return false;
  knownSsid = candidateSsid; knownPassword = candidatePassword;
  return true;
}
void finishTrial(bool success) {
  if (success) success = rememberWifi();
  if (LOCAL_TEST) localWifiResult = success ? "saved" : "failed";
  if (success) {
    if (trialRemote) { pendingResultId = wifiCommandId; pendingResultStatus = "applied"; }
    closePortal();
  } else {
    if (trialRemote) { pendingResultId = wifiCommandId; pendingResultStatus = "rolled_back"; }
    mqtt.disconnect(); secureClient.stop(); WiFi.disconnect(false, false);
    if (knownSsid.length()) WiFi.begin(knownSsid.c_str(), knownPassword.c_str());
    lastWifiTry = millis(); wifiLostAt = millis(); openPortal();
  }
  candidatePassword = ""; candidateSsid = ""; wifiCommandId = ""; trial = false;
}
void tickWifi(uint32_t now) {
  if (waitingToStartTrial && (int32_t)(now - trialStartAt) >= 0) {
    waitingToStartTrial = false; trial = true; trialStarted = now;
    outputsOff(); mqtt.disconnect(); secureClient.stop();
    WiFi.disconnect(false, false);
    WiFi.begin(candidateSsid.c_str(), candidatePassword.c_str());
    wasConnected = false; lastWifiTry = now;
  }
  bool connected = WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0,0,0,0);
  if (connected && !wasConnected) {
    if (!LOCAL_TEST) configTime(0, 0, "pool.ntp.org", "time.cloudflare.com");
    lastMqttTry = now - 10000; wasConnected = true;
  } else if (!connected && wasConnected) {
    wasConnected = false; wifiLostAt = now; mqtt.disconnect(); secureClient.stop();
  }
  if (trial) {
    // Remote change is committed only after verified MQTT reconnection.
    // Initial/local setup can be saved after acquiring an IP even if the Internet is unavailable.
    if (connected && ((!trialRemote && now - trialStarted > 3000) || (trialRemote && mqtt.connected()))) finishTrial(true);
    else if (now - trialStarted >= WIFI_TRIAL_MS) finishTrial(false);
    return;
  }
  if (waitingToStartTrial) return;
  if (connected) { if (portalOn) closePortal(); return; }
  if (!knownSsid.length() || now - wifiLostAt >= AP_AFTER_MS) openPortal();
  if (knownSsid.length() && now - lastWifiTry >= WIFI_RETRY_MS) {
    WiFi.begin(knownSsid.c_str(), knownPassword.c_str()); lastWifiTry = now;
  }
}
void tickMqtt(uint32_t now) {
  if (LOCAL_TEST) return;
  if (!configured() || WiFi.status() != WL_CONNECTED || time(nullptr) < 1700000000) return;
  if (!mqtt.connected() && now - lastMqttTry >= 10000) {
    lastMqttTry = now;
    String id = String("sg-") + DEVICE_ID;
    if (mqtt.connect(id.c_str(), MQTT_USER, MQTT_PASSWORD, (baseTopic+"/availability").c_str(), 1, true, "{\"online\":false}")) {
      mqtt.subscribe((baseTopic + "/command").c_str(), 1);
      mqtt.publish((baseTopic + "/availability").c_str(), "{\"online\":true}", true);
      telemetry();
    }
  }
  if (mqtt.connected()) {
    mqtt.loop();
    if (pendingResultId.length()) {
      StaticJsonDocument<256> d; d["id"] = pendingResultId; d["status"] = pendingResultStatus;
      d["detail"] = pendingResultStatus == "applied" ? "wifi_connected" : "wifi_failed";
      if (publishDoc(baseTopic + "/ack", d)) { pendingResultId = ""; pendingResultStatus = ""; }
    }
    if (now - lastTelemetry >= 15000) telemetry();
  }
}
void sensors(uint32_t now) {
  if (now - lastSensor >= 2500) {
    lastSensor = now;
    airC = dht.readTemperature(); humidity = dht.readHumidity();
    uint32_t raw = 0; for (int i = 0; i < 12; i++) raw += analogRead(SOIL_PIN);
    soilRaw = raw / 12;
    soilReady = true;
    soilPercent = SOIL_CALIBRATED && SOIL_DRY_RAW != SOIL_WET_RAW ?
      constrain(100.0f * (soilRaw - SOIL_DRY_RAW) / (SOIL_WET_RAW - SOIL_DRY_RAW), 0.0f, 100.0f) : NAN;
    lux = NAN;
    if (lightReady && light.measurementReady()) { float n = light.readLightLevel(); lux = n >= 0 ? n : NAN; }
    if (haveProbe && !probeWaiting) { probe.requestTemperatures(); probeRequested = now; probeWaiting = true; }
  }
  if (probeWaiting && now - probeRequested >= 800) {
    float n = probe.getTempC(probeAddress);
    probeC = n == DEVICE_DISCONNECTED_C || n < -55 || n > 125 ? NAN : n;
    probeWaiting = false;
  }
}
void lcdLine(int line, String s) {
  if (!lcdReady) return;
  s = s.substring(0, 16); while (s.length() < 16) s += ' ';
  lcd.setCursor(0, line); lcd.print(s);
}
void display(uint32_t now) {
  if (!lcdReady || now - lastLcd < 1000) return;
  lastLcd = now;
  if (LOCAL_TEST) {
    switch ((now / 4000) % 4) {
      case 0: lcdLine(0, "SmartGrow TEST"); lcdLine(1, "192.168.4.1"); break;
      case 1:
        lcdLine(0, "Udara:" + (isfinite(airC) ? String(airC,1) + "C" : String("--")));
        lcdLine(1, "RH:" + (isfinite(humidity) ? String(humidity,0) + "%" : String("--"))); break;
      case 2:
        lcdLine(0, "Tanah ADC:" + (soilReady ? String(soilRaw) : String("--")));
        lcdLine(1, "Probe:" + (isfinite(probeC) ? String(probeC,1) + "C" : String("--"))); break;
      default:
        lcdLine(0, "Lux:" + (isfinite(lux) ? String(lux,0) : String("--")));
        lcdLine(1, WiFi.status() == WL_CONNECTED ? "WiFi tersambung" : "WiFi opsional");
    }
    return;
  }
  if (!configured()) { lcdLine(0, "SmartGrow setup"); lcdLine(1, "Config required"); return; }
  if (portalOn) { lcdLine(0, "WiFi Setup aktif"); lcdLine(1, "192.168.4.1"); return; }
  if (trial) { lcdLine(0, "Mencoba WiFi..."); lcdLine(1, "Tunggu 60 detik"); return; }
  lcdLine(0, String("T:") + (isfinite(airC) ? String(airC, 1) : "--") + " H:" + (isfinite(humidity) ? String(humidity,0) : "--"));
  lcdLine(1, String(mqtt.connected() ? "Online " : "Offline ") + (SOIL_CALIBRATED ? "S:" + String(soilPercent,0) + "%" : "A:" + String(soilRaw)));
}
String serialFloat(float n, unsigned digits = 1) { return isfinite(n) ? String(n, digits) : String("--"); }
void serialReport(uint32_t now) {
  if (!LOCAL_TEST || now - lastSerialReport < 3000) return;
  lastSerialReport = now;
  String line = "[SENSOR] soil_raw=" + (soilReady ? String(soilRaw) : String("--")) +
    " | soil_pct=" + serialFloat(soilPercent) + " | udara_C=" + serialFloat(airC) +
    " | RH_pct=" + serialFloat(humidity) + " | probe_C=" + serialFloat(probeC) + " | lux=" + serialFloat(lux,0);
  Serial.println(line);
}
void setup() {
  Serial.begin(115200);
  digitalWrite(PUMP_PIN, PUMP_ACTIVE_LOW ? HIGH : LOW); pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(LAMP_PIN, LAMP_ACTIVE_LOW ? HIGH : LOW); pinMode(LAMP_PIN, OUTPUT);
  if (xTaskCreatePinnedToCore(relayTask, "relay_guard", 2048, nullptr, 3, nullptr, 1) != pdPASS) {
    Serial.println("Relay task failure; device halted with outputs OFF."); while(true) delay(1000);
  }
  pinMode(SOIL_PIN, INPUT); analogReadResolution(12); analogSetPinAttenuation(SOIL_PIN, ADC_11db);
  Wire.begin(SDA_PIN, SCL_PIN); Wire.setTimeOut(50);
  if (USE_LCD) { Wire.beginTransmission(LCD_ADDRESS); lcdReady = Wire.endTransmission() == 0; if (lcdReady) { lcd.init(); lcd.backlight(); } }
  if (USE_BH1750) lightReady = light.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
  dht.begin(); probe.begin(); probe.setWaitForConversion(false); haveProbe = probe.getAddress(probeAddress, 0);
  if (haveProbe) probe.setResolution(probeAddress, 12);
  prefs.begin(LOCAL_TEST ? "sg-localtest" : "smartgrow", false);
  StaticJsonDocument<256> stored;
  if (!deserializeJson(stored, prefs.getString("wifi", "{}"))) {
    knownSsid = String(stored["ssid"] | ""); knownPassword = String(stored["password"] | "");
  }
  WiFi.persistent(false); WiFi.setAutoReconnect(false);
  if (LOCAL_TEST) {
    char suffix[7]; snprintf(suffix, sizeof(suffix), "%06lX", (unsigned long)(ESP.getEfuseMac() & 0xFFFFFF));
    localId = suffix;
  }
  WiFi.setHostname((String("smartgrow-") + deviceId()).c_str()); WiFi.mode(WIFI_STA);
  if (LOCAL_TEST) {
    localApPassword = prefs.getString("ap_pass", "");
    if (localApPassword.length() < 8 || localApPassword.length() > 63) {
      localApPassword = randomHex().substring(0,12);
      if (prefs.putString("ap_pass", localApPassword) != localApPassword.length()) Serial.println("Password hotspot tidak tersimpan; baca lagi setelah restart.");
    }
  }
  bootId = randomHex(); baseTopic = String("smartgrow/v1/") + deviceId();
  secureClient.setCACert(MQTT_ROOT_CA); secureClient.setHandshakeTimeout(3); secureClient.setTimeout(1500);
  mqtt.setServer(MQTT_HOSTNAME, MQTT_PORT_NUMBER); mqtt.setCallback(onCommand);
  mqtt.setBufferSize(2048); mqtt.setSocketTimeout(2); mqtt.setKeepAlive(20);
  configurePortalRoutes(); wifiLostAt = millis(); lastWifiTry = millis() - WIFI_RETRY_MS;
  if (LOCAL_TEST) {
    openPortal();
    Serial.println("\n=== SmartGrow: UJI LOKAL TANPA VPS ===");
    Serial.println("Hotspot: " + apName());
    Serial.println("Password hotspot: " + apPassword());
    Serial.println("Buka http://192.168.4.1 dari HP yang tersambung ke hotspot.");
    Serial.printf("LCD=%s | BH1750=%s | DS18x20=%s\n", lcdReady ? "terdeteksi" : "tidak terdeteksi", lightReady ? "terdeteksi" : "tidak terdeteksi", haveProbe ? "terdeteksi" : "tidak terdeteksi");
    Serial.printf("Relay pompa=%s | lampu=%s (tes ON 2 detik)\n", ENABLE_PUMP ? "diizinkan" : "TERKUNCI", ENABLE_LAMP ? "diizinkan" : "TERKUNCI");
    Serial.println("-- berarti belum ada data/hasil tidak valid. Tanah persen perlu kalibrasi.");
  }
  if (!configured()) Serial.println("Copy/generated device_config.h required; relay outputs remain disabled by default.");
}
void loop() {
  uint32_t now = millis();
  if (configured()) { tickWifi(now); if (portalOn) { dns.processNextRequest(); portal.handleClient(); } tickMqtt(millis()); }
  sensors(millis()); display(millis()); serialReport(millis()); delay(2);
}
