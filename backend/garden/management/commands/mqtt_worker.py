import logging
import time
from datetime import timedelta
from django.conf import settings
from django.core.management.base import BaseCommand
from django.db import close_old_connections
from django.utils import timezone
from garden.mqtt import mqtt_client, ingest
from garden.models import Reading, RateBucket

log = logging.getLogger(__name__)

class Command(BaseCommand):
    help = "Run exactly one MQTT ingestion worker."
    def handle(self, *args, **options):
        c = mqtt_client("smartgrow-ingest")
        def on_connect(client, userdata, flags, reason_code, properties):
            if reason_code == 0:
                for kind in ("telemetry", "availability", "ack"):
                    client.subscribe(f"smartgrow/v1/+/{kind}", qos=1)
        def on_message(client, userdata, message):
            close_old_connections()
            try:
                ingest(message.topic, message.payload, message.retain)
            except Exception:
                # Do not log MQTT payloads (especially configuration secrets).
                log.error("Ingestion failed; message omitted")
            finally:
                close_old_connections()
        c.on_connect, c.on_message = on_connect, on_message
        c.reconnect_delay_set(min_delay=1, max_delay=30)
        c.connect_async(settings.MQTT_HOST, settings.MQTT_PORT, keepalive=30)
        c.loop_start()
        try:
            while True:
                time.sleep(30)
                close_old_connections()
                Reading.objects.filter(created_at__lt=timezone.now() - timedelta(days=30)).delete()
                RateBucket.objects.filter(started_at__lt=timezone.now() - timedelta(days=1)).delete()
        except KeyboardInterrupt:
            pass
        finally:
            c.disconnect()
            c.loop_stop()
