from datetime import timedelta
from django.contrib import messages
from django.contrib.auth.decorators import login_required
from django.contrib.auth.forms import UserCreationForm
from django.contrib.auth.hashers import check_password
from django.contrib.auth.views import LoginView
from django.db import transaction
from django.http import HttpResponse, JsonResponse
from django.shortcuts import render, redirect, get_object_or_404
from django.utils import timezone
from django.views.decorators.http import require_POST
from django.views.decorators.cache import never_cache
from django.views.decorators.debug import sensitive_post_parameters
from .models import Device, Reading, Command
from .mqtt import send_command
from .security import rate_allowed

class SignIn(LoginView):
    template_name = "registration/login.html"
    def post(self, request, *args, **kwargs):
        if not rate_allowed(request, "login", 10, 300):
            return HttpResponse("Terlalu banyak percobaan. Coba kembali dalam 5 menit.", status=429)
        return super().post(request, *args, **kwargs)

@sensitive_post_parameters("password1", "password2")
def register(request):
    if request.method == "POST" and not rate_allowed(request, "register", 5, 3600):
        return HttpResponse("Coba kembali nanti.", status=429)
    form = UserCreationForm(request.POST or None)
    if request.method == "POST" and form.is_valid():
        form.save()
        messages.success(request, "Akun berhasil dibuat. Silakan masuk.")
        return redirect("login")
    return render(request, "registration/register.html", {"form": form})

@login_required
def dashboard(request):
    return render(request, "garden/dashboard.html", {"devices": Device.objects.filter(owner=request.user).order_by("id")})

@login_required
@require_POST
@sensitive_post_parameters("code")
def claim(request):
    if not rate_allowed(request, "claim", 8, 300):
        return HttpResponse("Terlalu banyak percobaan aktivasi.", status=429)
    with transaction.atomic():
        device = Device.objects.select_for_update().filter(pk=request.POST.get("device_id", "")).first()
        code = request.POST.get("code", "")
        if not device or device.owner_id or not device.claim_hash or not check_password(code, device.claim_hash):
            messages.error(request, "ID/kode aktivasi tidak valid atau alat sudah diaktifkan.")
        else:
            device.owner = request.user
            device.claim_hash = ""
            device.claimed_at = timezone.now()
            device.save(update_fields=["owner", "claim_hash", "claimed_at"])
            messages.success(request, "Perangkat ditambahkan ke akun Anda.")
    return redirect("dashboard")

def owned(request, device_id):
    return get_object_or_404(Device, pk=device_id, owner=request.user)

@login_required
def detail(request, device_id):
    return render(request, "garden/detail.html", {"device": owned(request, device_id)})

@login_required
@never_cache
def data(request, device_id):
    d = owned(request, device_id)
    since = timezone.now() - timedelta(hours=1)
    readings = list(Reading.objects.filter(device=d, created_at__gte=since).order_by("-created_at")[:240])
    commands = list(Command.objects.filter(device=d).order_by("-created_at")[:8])
    result = []
    for c in commands:
        status = c.status
        if status in ("sending", "sent", "delivery_unknown") and c.expires_at < timezone.now():
            status = "no_confirmation"
        if status == "accepted" and c.created_at < timezone.now() - timedelta(minutes=3):
            status = "no_confirmation"
        result.append({"id": str(c.id), "summary": c.summary, "status": status, "detail": c.detail})
    return JsonResponse({"id": d.id, "online": d.online, "last_seen": d.last_seen, "state": d.state,
        "readings": [{"at": r.created_at, "values": r.values} for r in reversed(readings)], "commands": result})

@login_required
@require_POST
def relay(request, device_id):
    d = owned(request, device_id)
    if not rate_allowed(request, "relay", 30, 60):
        return HttpResponse("Terlalu banyak perintah.", status=429)
    target, action = request.POST.get("target"), request.POST.get("action")
    if target not in ("pump", "lamp") or action not in ("on", "off"):
        return HttpResponse("Perintah tidak valid.", status=400)
    try:
        seconds = int(request.POST.get("seconds", "0")) if action == "on" else 0
    except ValueError:
        return HttpResponse("Durasi tidak valid.", status=400)
    maximum = 60 if target == "pump" else 900
    if action == "on" and not 1 <= seconds <= maximum:
        return HttpResponse(f"Durasi harus 1–{maximum} detik.", status=400)
    if action == "on" and not d.state.get(f"{target}_enabled", False):
        messages.error(request, "Kontrol ini belum diaktifkan. Hubungi pengelola SmartGrow.")
        return redirect("device", device_id=d.id)
    try:
        send_command(d, request.user, "relay", {"target": target, "on": action == "on", "duration_s": seconds},
                     f"{'Pompa' if target == 'pump' else 'Lampu'} {action.upper()} ({seconds} dtk)")
        messages.info(request, "Perintah dikirim. Tunggu konfirmasi dari perangkat.")
    except ValueError as e:
        messages.error(request, str(e))
    return redirect("device", device_id=d.id)

@login_required
@require_POST
@sensitive_post_parameters("wifi_password")
def wifi(request, device_id):
    d = owned(request, device_id)
    if not rate_allowed(request, "wifi", 3, 300):
        return HttpResponse("Tunggu beberapa menit sebelum mengganti Wi-Fi lagi.", status=429)
    ssid, password = request.POST.get("ssid", ""), request.POST.get("wifi_password", "")
    if not 1 <= len(ssid.encode()) <= 32 or not 8 <= len(password.encode()) <= 63 or "\x00" in ssid + password:
        messages.error(request, "SSID 1–32 byte dan kata sandi Wi-Fi 8–63 byte diperlukan.")
    else:
        try:
            send_command(d, request.user, "wifi", {"ssid": ssid, "password": password}, "Mengganti Wi-Fi")
            messages.info(request, "Penggantian diminta. Alat mungkin offline sementara; tunggu konfirmasi.")
        except ValueError as e:
            messages.error(request, str(e))
    return redirect("device", device_id=d.id)
