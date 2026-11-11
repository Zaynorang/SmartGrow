import json
import math
import re
import uuid
from datetime import timedelta
import paho.mqtt.client as mqtt
from django.conf import settings
from django.utils import timezone
from .models import Device, Reading, Command

TOPIC_RE = re.compile(r"^smartgrow/v1/([A-Za-z0-9_-]{3,40})/(telemetry|availability|ack)$")
RANGES = {"soil_raw": (0, 4095), "soil_pct": (0, 100), "air_c": (-40, 85),
          "humidity_pct": (0, 100), "probe_c": (-55, 125), "lux": (0, 150000), "rssi": (-130, 0)}

def mqtt_client(client_id):
    c = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=client_id, clean_session=True)
    c.username_pw_set(settings.MQTT_USERNAME, settings.MQTT_PASSWORD)
    c.connect_timeout = 4
    if settings.MQTT_TLS:
        c.tls_set(ca_certs=settings.MQTT_CA)
    return c

def send_command(device, user, kind, payload, summary):
    if not device.online:
        raise ValueError("Alat offline. Sambungkan alat terlebih dahulu.")
    now = timezone.now()
    cmd = Command.objects.create(device=device, user=user, kind=kind, summary=summary,
                                 expires_at=now + timedelta(seconds=30))
    wire = {**payload, "id": str(cmd.id), "kind": kind, "boot_id": device.boot_id,
            "issued_at": int(now.timestamp()), "expires_at": int(cmd.expires_at.timestamp())}
    c = mqtt_client(f"web-{uuid.uuid4().hex[:16]}")
    try:
        c.connect(settings.MQTT_HOST, settings.MQTT_PORT, keepalive=20)
        c.loop_start()
        info = c.publish(f"smartgrow/v1/{device.id}/command", json.dumps(wire), qos=1, retain=False)
        info.wait_for_publish(timeout=4)
        if not info.is_published():
            raise TimeoutError("publish timeout")
        # Do not overwrite a fast acknowledgement received by mqtt_worker.
        Command.objects.filter(pk=cmd.pk, status="sending").update(status="sent")
    except Exception:
        Command.objects.filter(pk=cmd.pk, status="sending").update(status="delivery_unknown")
        # A timeout does not prove the message wasn't delivered. Await device ACK.
    finally:
        c.disconnect()
        c.loop_stop()
        wire.clear()
    cmd.refresh_from_db()
    return cmd

def ingest(topic, payload, retained=False):
    match = TOPIC_RE.fullmatch(topic)
    if not match or len(payload) > 4096:
        return False
    try:
        body = json.loads(payload)
    except (ValueError, UnicodeDecodeError):
        return False
    if not isinstance(body, dict):
        return False
    device = Device.objects.filter(pk=match[1]).first()
    if not device:
        return False
    kind, now = match[2], timezone.now()
    if kind == "availability":
        # Retained birth messages cannot prove current liveness.
        if body.get("online") is False:
            Device.objects.filter(pk=device.pk).update(connected=False)
        return True
    if retained:
        return False
    if kind == "telemetry":
        boot = body.get("boot_id")
        if not isinstance(boot, str) or not re.fullmatch(r"[a-f0-9]{16,40}", boot):
            return False
        clean = {}
        for name, (low, high) in RANGES.items():
            value = body.get(name)
            if value is not None and (type(value) not in (int, float) or not math.isfinite(value) or not low <= value <= high):
                return False
            clean[name] = value
        for name in ("pump", "lamp", "pump_enabled", "lamp_enabled"):
            if type(body.get(name)) is not bool:
                return False
            clean[name] = body[name]
        clean["firmware"] = str(body.get("firmware", ""))[:30]
        # Every telemetry message contains complete state, not a partial merge.
        Device.objects.filter(pk=device.pk).update(state=clean, connected=True, last_seen=now, boot_id=boot)
        Reading.objects.create(device=device, values=clean)
        return True
    if kind == "ack":
        status = body.get("status")
        if status not in ("accepted", "applied", "rejected", "rolled_back"):
            return False
        try:
            command_id = uuid.UUID(str(body.get("id")))
        except (ValueError, TypeError):
            return False
        detail = body.get("detail", "")
        allowed = {"", "relay_set", "outputs_locked", "invalid_command", "expired", "wrong_boot",
                   "wifi_testing", "wifi_connected", "wifi_failed", "busy", "duplicate"}
        if detail not in allowed:
            detail = "invalid_command"
        Command.objects.filter(pk=command_id, device=device).exclude(
            status__in=["applied", "rejected", "rolled_back"]).update(status=status, detail=detail)
        return True
    return False
