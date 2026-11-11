import json
from datetime import timedelta
from unittest.mock import patch, MagicMock
from django.contrib.auth import get_user_model
from django.contrib.auth.hashers import make_password
from django.test import TestCase, Client, override_settings
from django.utils import timezone
from .models import Device, Reading, Command
from .mqtt import ingest, send_command

@override_settings(PASSWORD_HASHERS=["django.contrib.auth.hashers.MD5PasswordHasher"], SECURE_SSL_REDIRECT=False)
class SmartGrowTests(TestCase):
    def setUp(self):
        User=get_user_model()
        self.a=User.objects.create_user("alice",password="test-secret-123")
        self.b=User.objects.create_user("bob",password="test-secret-456")
        self.d=Device.objects.create(id="SG0001",owner=self.a,connected=True,last_seen=timezone.now(),
            boot_id="a"*32,state={"pump_enabled":True,"lamp_enabled":False,"pump":False,"lamp":False})
        self.client.force_login(self.a)

    def telemetry(self, **changes):
        d={"boot_id":"a"*32,"firmware":"0.1.0","soil_raw":2000,"soil_pct":None,"air_c":29.0,
           "humidity_pct":70,"probe_c":27.5,"lux":400,"rssi":-60,"pump":False,"lamp":False,
           "pump_enabled":True,"lamp_enabled":False}
        d.update(changes)
        return json.dumps(d).encode()

    def test_owner_only_reads_and_controls(self):
        self.client.force_login(self.b)
        for url in ["/devices/SG0001/","/devices/SG0001/data/"]:
            self.assertEqual(self.client.get(url).status_code,404)
        with patch("garden.views.send_command") as send:
            for endpoint in ("relay","wifi"):
                self.assertEqual(self.client.post(f"/devices/SG0001/{endpoint}/",{}).status_code,404)
            send.assert_not_called()

    def test_claim_is_single_use_and_cannot_steal(self):
        Device.objects.create(id="SG0002",claim_hash=make_password("activation-very-long-secret"))
        r=self.client.post("/claim/",{"device_id":"SG0002","code":"activation-very-long-secret"})
        self.assertEqual(r.status_code,302)
        d=Device.objects.get(pk="SG0002");self.assertEqual(d.owner,self.a);self.assertEqual(d.claim_hash,"")
        self.client.force_login(self.b)
        self.client.post("/claim/",{"device_id":"SG0002","code":"activation-very-long-secret"})
        d.refresh_from_db();self.assertEqual(d.owner,self.a)

    def test_wrong_claim_and_rate_limit(self):
        for _ in range(8): self.client.post("/claim/",{"device_id":"unknown","code":"bad"})
        self.assertEqual(self.client.post("/claim/",{}).status_code,429)

    def test_csrf_protects_relay(self):
        c=Client(enforce_csrf_checks=True);c.force_login(self.a)
        self.assertEqual(c.post("/devices/SG0001/relay/",{"target":"pump","action":"on","seconds":10}).status_code,403)

    def test_stale_or_disconnected_device_rejects_commands(self):
        self.d.last_seen=timezone.now()-timedelta(seconds=46)
        with self.assertRaises(ValueError): send_command(self.d,self.a,"relay",{},"test")
        self.d.last_seen=timezone.now();self.d.connected=False
        with self.assertRaises(ValueError): send_command(self.d,self.a,"relay",{},"test")

    def test_relay_range_and_locked_output(self):
        with patch("garden.views.send_command") as send:
            r=self.client.post("/devices/SG0001/relay/",{"target":"pump","action":"on","seconds":61})
            self.assertEqual(r.status_code,400)
            self.client.post("/devices/SG0001/relay/",{"target":"lamp","action":"on","seconds":30})
            send.assert_not_called()
            self.client.post("/devices/SG0001/relay/",{"target":"pump","action":"off"})
            self.assertEqual(send.call_args.args[3],{"target":"pump","on":False,"duration_s":0})

    def test_telemetry_invalid_values_and_retained_data_rejected(self):
        t="smartgrow/v1/SG0001/telemetry"
        self.assertFalse(ingest(t,self.telemetry(),retained=True))
        for change in ({"lux":float("nan")},{"soil_raw":5000},{"humidity_pct":True},{"pump":"on"},{"boot_id":"bad"}):
            self.assertFalse(ingest(t,self.telemetry(**change)))
        self.assertEqual(Reading.objects.count(),0)

    def test_telemetry_null_replaces_old_value(self):
        t="smartgrow/v1/SG0001/telemetry"
        self.assertTrue(ingest(t,self.telemetry()))
        self.assertTrue(ingest(t,self.telemetry(air_c=None)))
        self.d.refresh_from_db();self.assertIsNone(self.d.state["air_c"]);self.assertTrue(self.d.online)
        self.assertEqual(Reading.objects.count(),2)

    def test_retained_birth_does_not_resurrect_device(self):
        self.d.connected=False;self.d.save()
        ingest("smartgrow/v1/SG0001/availability",b'{"online":true}',retained=True)
        self.d.refresh_from_db();self.assertFalse(self.d.online)

    def test_last_will_marks_offline(self):
        ingest("smartgrow/v1/SG0001/availability",b'{"online":false}',retained=True)
        self.d.refresh_from_db();self.assertFalse(self.d.online)

    def test_wrong_device_ack_cannot_update_command(self):
        other=Device.objects.create(id="SG0002",owner=self.b)
        cmd=Command.objects.create(device=self.d,user=self.a,kind="relay",summary="test",expires_at=timezone.now())
        ingest(f"smartgrow/v1/{other.id}/ack",json.dumps({"id":str(cmd.pk),"status":"applied"}).encode())
        cmd.refresh_from_db();self.assertEqual(cmd.status,"sending")

    def test_late_ack_cannot_downgrade_applied(self):
        cmd=Command.objects.create(device=self.d,user=self.a,kind="wifi",summary="test",expires_at=timezone.now())
        for status in ("applied","accepted","rejected"):
            ingest("smartgrow/v1/SG0001/ack",json.dumps({"id":str(cmd.pk),"status":status}).encode())
        cmd.refresh_from_db();self.assertEqual(cmd.status,"applied")

    def test_publish_has_ttl_boot_id_no_retention_and_no_db_password(self):
        c=MagicMock();c.publish.return_value.is_published.return_value=True
        with patch("garden.mqtt.mqtt_client",return_value=c):
            cmd=send_command(self.d,self.a,"wifi",{"ssid":"My WiFi","password":"wifi-secret-value"},"Mengganti Wi-Fi")
        args,kwargs=c.publish.call_args
        self.assertFalse(kwargs["retain"]);self.assertEqual(kwargs["qos"],1)
        wire=json.loads(args[1]);self.assertEqual(wire["boot_id"],self.d.boot_id)
        self.assertEqual(wire["expires_at"]-wire["issued_at"],30)
        self.assertNotIn("wifi-secret-value",str(Command.objects.values().get(pk=cmd.pk)))

    def test_delivery_timeout_is_not_reported_as_applied(self):
        c=MagicMock();c.publish.return_value.wait_for_publish.side_effect=TimeoutError()
        with patch("garden.mqtt.mqtt_client",return_value=c):
            cmd=send_command(self.d,self.a,"relay",{},"test")
        self.assertEqual(cmd.status,"delivery_unknown")

    def test_wifi_validates_utf8_byte_lengths(self):
        with patch("garden.views.send_command") as send:
            self.client.post("/devices/SG0001/wifi/",{"ssid":"🌱"*9,"wifi_password":"password123"})
            send.assert_not_called()

    def test_html_pages_and_data_schema(self):
        for url in ("/","/devices/SG0001/","/login/","/register/"):
            self.assertEqual(self.client.get(url).status_code,200)
        r=self.client.get("/devices/SG0001/data/")
        self.assertTrue(r.json()["online"])
        self.assertIn("no-store",r.headers["Cache-Control"])

    def test_unsafe_request_method_rejected(self):
        for endpoint in ("relay","wifi"):
            self.assertEqual(self.client.get(f"/devices/SG0001/{endpoint}/").status_code,405)
