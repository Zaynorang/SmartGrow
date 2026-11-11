#pragma once
// ESP32 classic DevKit / WROOM. Dxx means GPIOxx, not physical pin number.
constexpr int SOIL_PIN = 34;
constexpr int DHT_PIN = 5;
constexpr int PROBE_PIN = 4;
constexpr int SDA_PIN = 21;
constexpr int SCL_PIN = 22;
constexpr int PUMP_PIN = 14;
constexpr int LAMP_PIN = 26;
constexpr unsigned char LCD_ADDRESS = 0x27; // Common alternatives: 0x3F.
constexpr bool USE_LCD = true;
constexpr bool USE_BH1750 = true;
constexpr bool PUMP_ACTIVE_LOW = true;
constexpr bool LAMP_ACTIVE_LOW = true;
// Enable only AFTER checking polarity with pump/lamp loads disconnected.
constexpr bool ENABLE_PUMP = false;
constexpr bool ENABLE_LAMP = false;
constexpr unsigned PUMP_MAX_SECONDS = 60;
constexpr unsigned LAMP_MAX_SECONDS = 900;
// No invented moisture percentage: replace values after dry/wet calibration.
constexpr bool SOIL_CALIBRATED = false;
constexpr int SOIL_DRY_RAW = 3000;
constexpr int SOIL_WET_RAW = 1200;
