import uuid
from datetime import timedelta
from django.conf import settings
from django.core.validators import RegexValidator
from django.db import models
from django.utils import timezone

DEVICE_ID = RegexValidator(r"^[A-Za-z0-9_-]{3,40}$", "ID perangkat tidak valid.")

class Device(models.Model):
    id = models.CharField(primary_key=True, max_length=40, validators=[DEVICE_ID])
    name = models.CharField(max_length=80, default="SmartGrow")
    owner = models.ForeignKey(settings.AUTH_USER_MODEL, null=True, blank=True, on_delete=models.PROTECT)
    claim_hash = models.CharField(max_length=256, blank=True)
    claimed_at = models.DateTimeField(null=True, blank=True)
    last_seen = models.DateTimeField(null=True, blank=True)
    connected = models.BooleanField(default=False)
    boot_id = models.CharField(max_length=40, blank=True)
    state = models.JSONField(default=dict)

    @property
    def online(self):
        return bool(self.connected and self.last_seen and self.boot_id and
                    self.last_seen > timezone.now() - timedelta(seconds=settings.DEVICE_ONLINE_SECONDS))

class Reading(models.Model):
    device = models.ForeignKey(Device, on_delete=models.CASCADE)
    created_at = models.DateTimeField(default=timezone.now, db_index=True)
    values = models.JSONField(default=dict)
    class Meta:
        indexes = [models.Index(fields=["device", "created_at"])]

class Command(models.Model):
    id = models.UUIDField(primary_key=True, default=uuid.uuid4, editable=False)
    device = models.ForeignKey(Device, on_delete=models.CASCADE)
    user = models.ForeignKey(settings.AUTH_USER_MODEL, on_delete=models.PROTECT)
    kind = models.CharField(max_length=12)
    # Only non-secret metadata. Wi-Fi password must never enter this field.
    summary = models.CharField(max_length=100)
    status = models.CharField(max_length=24, default="sending")
    detail = models.CharField(max_length=100, blank=True)
    created_at = models.DateTimeField(default=timezone.now)
    expires_at = models.DateTimeField()

class RateBucket(models.Model):
    key = models.CharField(max_length=64, primary_key=True)
    started_at = models.DateTimeField(default=timezone.now)
    hits = models.PositiveIntegerField(default=0)
