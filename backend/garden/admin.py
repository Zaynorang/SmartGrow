from django.contrib import admin
from .models import Device, Command

@admin.register(Device)
class DeviceAdmin(admin.ModelAdmin):
    list_display = ("id", "name", "owner", "last_seen", "connected")
    exclude = ("claim_hash",)
    readonly_fields = ("last_seen", "state", "boot_id", "connected", "claimed_at")

@admin.register(Command)
class CommandAdmin(admin.ModelAdmin):
    list_display = ("id", "device", "user", "summary", "status", "created_at")
    def has_add_permission(self, request): return False
    def has_change_permission(self, request, obj=None): return False
