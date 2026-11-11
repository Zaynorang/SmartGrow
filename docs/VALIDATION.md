# Verifikasi SmartGrow 0.1.1

Tanggal: 20 Oktober 2026. Hasil ini berlaku untuk source code dalam paket ini.

## Yang telah dijalankan

| Pemeriksaan | Hasil dan cakupan |
|---|---|
| Kompilasi mode uji lokal | **Lulus** environment `esp32-local` tanpa `device_config.h`, board ESP32 DevKit klasik, Espressif32 6.12.0 / Arduino ESP32 2.0.17, partisi `huge_app`. Flash aplikasi 993.473 / 3.145.728 byte (31,6%); RAM statis 48.180 / 327.680 byte (14,7%). |
| Kompilasi mode server | **Lulus** environment `esp32dev`. Build menggunakan header identitas uji dan root CA publik agar jalur jaringan ikut dikompilasi. Flash aplikasi 999.157 / 3.145.728 byte (31,8%); RAM statis 48.480 / 327.680 byte (14,8%). Angka RAM ini tidak mengukur penggunaan heap saat berjalan. |
| Backend Django | **17 tes lulus** memakai database SQLite sementara. Mencakup pembatasan pemilik, klaim satu kali, CSRF, pembatasan percobaan, status offline, validasi durasi/kunci relay, validasi telemetri, dan konfirmasi perintah. |
| Perintah MQTT | Tes backend memeriksa topik, TTL, `boot_id`, pesan tidak retained, tidak menyimpan password Wi-Fi ke database, status pengiriman yang belum pasti, dan konfirmasi yang datang terlambat/salah perangkat. Koneksi broker pada tes ini menggunakan mock. |
| Migrasi | `makemigrations --check --dry-run`: tidak ada perubahan model yang belum dimigrasikan. |
| Logika C++ | Tes native `ControlCore.h` lulus dengan `g++ -std=c++11 -Wall -Wextra -Werror`: batas durasi, output terkunci, pemutusan timer termasuk rollover `millis()`, dan masa berlaku perintah. |
| Sintaks | Python `compileall` dan JavaScript `node --check` lulus. |
| Konfigurasi Compose | YAML dapat dibaca dan pemetaan port diperiksa; hanya 80, 443, 8883 yang dipublikasikan. Ini bukan hasil `docker compose up`. |
| Tampilan dashboard | Hasil render template Django dibuka melalui pratinjau lokal dengan data uji: kartu sensor, grafik, status, kontrol terkunci, dan formulir Wi-Fi terlihat. Ini pemeriksaan tampilan, bukan koneksi browser ke ESP32. |
| Halaman uji ESP32 | HTML/JS dari `local_test_page.h` dibuka dalam pratinjau lokal dengan fixture sensor. Nilai sensor, persentase belum dikalibrasi, deteksi awal, dan kunci relay tampil sesuai. Ketika fixture data dibuat tidak tersedia, peringatan koneksi terputus muncul dan seluruh tombol dinonaktifkan. Tidak ada ESP32 yang dihubungkan ke pratinjau tersebut. |

Pada pembaruan 0.1.1, kedua environment firmware dikompilasi, sintaks JavaScript halaman uji diperiksa, dan tes timer C++ dijalankan kembali. Source backend tidak diubah; hasil 17 tes backend di atas berasal dari verifikasi versi awal pada tanggal yang sama.

Build penuh awal menampilkan peringatan `extra tokens at end of #undef directive` dari library OneWire 2.3.8. Build tetap berhasil. Library pihak ketiga tidak dimodifikasi untuk menyembunyikan peringatan tersebut.

`manage.py check --deploy` dengan konfigurasi produksi menghasilkan dua peringatan: `security.W005` dan `security.W021`. HSTS sudah aktif untuk host website, tetapi `includeSubDomains` dan `preload` sengaja tidak dinyalakan sebelum pemilik memastikan seluruh kebijakan domainnya. Tidak ada error atau peringatan lain pada pemeriksaan tersebut.

## Yang belum diuji

- ESP32 dan sensor fisik, tipe board/flash sebenarnya, polaritas relay, kondisi GPIO saat listrik masuk, serta catu daya/beban.
- Koneksi TLS dan ACL ke broker Mosquitto nyata, pengiriman MQTT berulang, restart broker, serta pergantian/fallback Wi-Fi di jaringan fisik.
- Menjalankan Compose, PostgreSQL, nginx, Certbot, dan skrip provisioning pada VPS. Docker tidak tersedia di lingkungan pengujian ini.
- Pengujian konkurensi pada PostgreSQL, kapasitas banyak perangkat, operasi beberapa hari, gangguan daya, dan pemulihan backup.
- Firmware OTA, secure boot, enkripsi flash, reset akun melalui email, dan proses produksi komersial.

Header uji, binary hasil kompilasi, database uji, dan rahasia server tidak disertakan. Buat identitas unik untuk setiap alat menggunakan prosedur dalam `README.md`, lalu kompilasi untuk alat tersebut. Tidak ada akun produksi atau layanan VPS yang dibuat oleh paket ini.

## Pemeriksaan berikutnya

Ikuti bagian “Uji bertahap sebelum memasang beban” dalam `README.md`. Biarkan `ENABLE_PUMP` dan `ENABLE_LAMP` bernilai `false` sampai wiring dan polaritas benar-benar diperiksa. Konfirmasi jenis lampu terlebih dahulu; BH1750 mengukur cahaya tampak dan tidak mengukur UV.

Kompilasi dan tes perangkat lunak yang lulus belum cukup untuk menyatakan produk siap dijual. Jalankan uji integrasi pada satu alat dan satu VPS terlebih dahulu, kemudian uji beberapa perangkat dengan pemilik akun berbeda.
