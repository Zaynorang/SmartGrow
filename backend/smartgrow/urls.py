from django.contrib import admin
from django.contrib.auth.views import LogoutView
from django.urls import path
from garden import views

urlpatterns = [
    path("admin/", admin.site.urls),
    path("login/", views.SignIn.as_view(), name="login"),
    path("logout/", LogoutView.as_view(), name="logout"),
    path("register/", views.register, name="register"),
    path("", views.dashboard, name="dashboard"),
    path("claim/", views.claim, name="claim"),
    path("devices/<str:device_id>/", views.detail, name="device"),
    path("devices/<str:device_id>/data/", views.data, name="data"),
    path("devices/<str:device_id>/relay/", views.relay, name="relay"),
    path("devices/<str:device_id>/wifi/", views.wifi, name="wifi"),
]
