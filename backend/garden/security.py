import hashlib
from datetime import timedelta
from django.db import transaction
from django.utils import timezone
from .models import RateBucket

def rate_allowed(request, scope, limit=10, seconds=60):
    # nginx overwrites X-Real-IP. The Django service has no public host port.
    ip = request.META.get("HTTP_X_REAL_IP", request.META.get("REMOTE_ADDR", "unknown"))
    who = str(request.user.pk) if getattr(request, "user", None) and request.user.is_authenticated else ip
    key = hashlib.sha256(f"{scope}:{who}".encode()).hexdigest()
    with transaction.atomic():
        bucket, _ = RateBucket.objects.select_for_update().get_or_create(key=key)
        now = timezone.now()
        if now - bucket.started_at >= timedelta(seconds=seconds):
            bucket.started_at, bucket.hits = now, 0
        if bucket.hits >= limit:
            return False
        bucket.hits += 1
        bucket.save()
        return True
