import json
import sys
from django.core.management.base import BaseCommand, CommandError
from django.contrib.auth.hashers import make_password
from garden.models import Device, DEVICE_ID

class Command(BaseCommand):
    help = "Create an unclaimed device from JSON on stdin; never pass activation codes via argv."
    def handle(self, *args, **options):
        data = json.load(sys.stdin)
        device_id, code = data["device_id"], data["claim_code"]
        DEVICE_ID(device_id)
        if len(code) < 20 or Device.objects.filter(pk=device_id).exists():
            raise CommandError("Device exists or claim code is too short.")
        Device.objects.create(id=device_id, name=data.get("name", "SmartGrow")[:80], claim_hash=make_password(code))
        self.stdout.write(f"Created {device_id}")
