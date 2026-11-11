# Kontrak MQTT SmartGrow v1

Semua alat mempunyai ID unik. Firmware koneksi ke broker menggunakan MQTT 3.1.1, TLS server-verified, akun per perangkat, dan clean session. Backend melakukan otorisasi pemilik sebelum publikasi. Database tidak menyimpan kata sandi Wi-Fi dari form.

| Topik | Pengirim | Penerima | Retained |
|---|---|---|---|
| `smartgrow/v1/{id}/telemetry` | Alat | Worker backend | Tidak |
| `smartgrow/v1/{id}/availability` | Alat / Last Will | Worker backend | Ya |
| `smartgrow/v1/{id}/command` | Backend | Alat yang dituju | **Tidak** |
| `smartgrow/v1/{id}/ack` | Alat | Worker backend | Tidak |

Telemetri dikirim setiap 15 detik dengan field lengkap. Sensor gagal/tidak terkalibrasi menggunakan `null`. Contoh ilustratif:

```json
{"boot_id":"f07e5555000000000000000000000001","firmware":"0.1.0","soil_raw":2100,"soil_pct":null,"air_c":28.5,"humidity_pct":71,"probe_c":26.7,"lux":1240,"rssi":-53,"pump":false,"lamp":false,"pump_enabled":false,"lamp_enabled":false}
```

Status `pump/lamp` adalah state output yang dikelola ESP32, bukan bukti fisik aliran air/penyinaran. Status online memerlukan telemetri baru; retained birth lama tidak cukup. Pesan availability `{"online":false}` dapat segera menandai offline. Tidak adanya heartbeat selama 45 detik juga menandai offline.

Perintah berisi UUID, `boot_id` yang didapat dari telemetri terbaru, waktu Unix `issued_at`, dan `expires_at` (30 detik setelah diterbitkan). Contoh ON pompa:

```json
{"id":"8200a8ef-6253-4ed3-ad36-a20f2a7b9d37","kind":"relay","boot_id":"f07e5555000000000000000000000001","issued_at":1792476000,"expires_at":1792476030,"target":"pump","on":true,"duration_s":10}
```

OFF menggunakan `on:false`, `duration_s:0`. Alat menolak boot ID berbeda, waktu kedaluwarsa, durasi melebihi batas, dan keluaran yang dikunci. UUID terakhir dideduplikasi agar pengiriman QoS 1 berulang tidak memperpanjang timer. Buffer deduplikasi memuat 32 UUID; backend dibatasi 30 perintah relay per pengguna per menit. Perintah tidak dipersistenkan atau di-retain broker.

Wi-Fi: `kind:"wifi"`, `ssid`, dan `password` bersama metadata umum. Password hanya untuk pengiriman sementara; tidak masuk ringkasan DB, ACK, telemetri, atau log. Backend memang dapat mengakses password saat memprosesnya; ini bukan enkripsi end-to-end terhadap operator server. Firmware menyimpan pasangan SSID/password di satu key NVS setelah sukses.

ACK berisi `id`, `status`, `detail`. Status final `applied`, `rejected`, `rolled_back`; `accepted` berarti sedang diproses. ACK dari perangkat lain tidak dapat mengubah perintah milik alat berbeda. ACK terlambat tidak menurunkan status final. Pergantian Wi-Fi remote baru `applied` setelah tersambung MQTT di jaringan baru. Saat gagal, `rolled_back` dapat dilaporkan setelah server kembali terjangkau; bila tidak, UI tetap menyatakan belum ada konfirmasi.

PubSubClient mengirim telemetri dan ACK QoS 0. Kehilangan ACK karena jaringan bukan bukti bahwa aksi tidak berjalan; UI tidak mengklaim berhasil tanpa konfirmasi. Jika tidak ada ACK, pengguna memeriksa status terkini sebelum mengulangi tindakan. Keluaran selalu memiliki batas waktu lokal.

Pengaturan lokal menggunakan HTTP pada hotspot WPA2 berkunci unik, tidak mengirim password Wi-Fi ke VPS. Form memakai token acak per pembukaan portal. Fallback dipicu oleh putus sambungan Wi-Fi ke router, bukan semata server/internet tidak tersedia.
