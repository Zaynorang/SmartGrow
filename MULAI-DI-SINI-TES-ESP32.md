# Tes SmartGrow pada ESP32 tanpa VPS

Tujuan tahap ini: memastikan sensor, LCD, ESP32, dan koneksi Wi-Fi bekerja sebelum menyiapkan server. Tidak diperlukan VPS, Mosquitto, akun web, domain, atau `device_config.h`.

## 1. Persiapan

- Asumsi board: ESP32 DevKit/WROOM klasik, flash 4 MB. Profil ini tidak untuk ESP32-C3/S3.
- Gunakan kabel USB yang mendukung data. Instal VS Code dan ekstensi **PlatformIO IDE** dari PlatformIO.
- Ekstrak paket ZIP ke folder biasa, misalnya `C:\SmartGrow`. Jangan membuka proyek langsung dari ZIP.
- Pastikan firmware lama sudah dicadangkan jika masih diperlukan. Upload mengganti program ESP32.
- Untuk tes pertama, lepas beban pompa/lampu dari relay. Boleh melepas seluruh modul relay saat menguji sensor. Polaritas modul relay belum diketahui; kondisi OFF fisik tidak bisa dijamin hanya oleh nilai bawaan kode.

## 2. Buka proyek dan upload

1. Di VS Code, pilih **File → Open Folder** lalu buka folder **firmware** di dalam SmartGrow. Folder itu harus langsung berisi `platformio.ini`, `include`, dan `src`.
2. Sambungkan ESP32 melalui USB. Di Windows, lihat **Device Manager → Ports (COM & LPT)** untuk mengetahui port, misalnya COM5.
3. Klik ikon PlatformIO di sidebar. Buka **PROJECT TASKS → esp32-local → General → Build**. Tunggu library dan toolchain selesai diunduh pada build pertama.
4. Jika Build menampilkan `SUCCESS`, pilih **Upload** pada environment **esp32-local** yang sama.
5. Setelah berhasil, pilih **Monitor**. Baud rate sudah 115200. Tekan tombol **EN/RESET** satu kali agar informasi awal terlihat kembali.

Pilih **esp32-local**, bukan `esp32dev`. Environment `esp32-local` sudah menjadi pilihan default di `platformio.ini`. Jangan membuat proyek baru atau memasukkan file ke Arduino IDE; buka proyek PlatformIO yang sudah ada.

Jika port tidak terdeteksi otomatis, tambahkan di bawah `[env:esp32-local]` pada `platformio.ini` (ganti COM5):

```ini
upload_port = COM5
monitor_port = COM5
```

Alternatif dari terminal PlatformIO, dengan direktori kerja folder `firmware`:

```bash
pio run -e esp32-local
pio run -e esp32-local --target upload --upload-port COM5
pio device monitor -e esp32-local --port COM5 --baud 115200
```

## 3. Baca hasil sensor melalui Serial Monitor

Saat boot, alat mencetak nama hotspot dan kata sandinya. Nama hotspot berbentuk `SmartGrow-Test-XXXXXX`, dengan akhiran dari identitas chip. Password dibuat acak pada pemakaian pertama dan disimpan pada ESP32. Tekan EN/RESET setelah Monitor terbuka jika informasi terlewat.

Setiap tiga detik muncul baris `[SENSOR]` berisi:

| Kolom | Arti |
|---|---|
| `soil_raw` | Nilai mentah ADC tanah 0–4095, GPIO34 |
| `soil_pct` | Persentase hasil kalibrasi; `--` sebelum kalibrasi |
| `udara_C` | Suhu udara dari DHT11, GPIO5 |
| `RH_pct` | Kelembapan udara dari DHT11 |
| `probe_C` | Suhu probe DS1820/DS18x20, GPIO4 |
| `lux` | Intensitas cahaya tampak dari BH1750, I²C SDA21/SCL22 |

`--` berarti belum ada data atau pembacaan tidak valid, bukan suhu/kelembapan nol. GPIO analog yang tidak tersambung masih dapat menghasilkan angka: nilai ADC saja bukan bukti sensor tanah sudah terpasang benar.

LCD menampilkan empat tampilan secara bergantian setiap empat detik: alamat halaman lokal; suhu/kelembapan udara; ADC tanah/suhu probe; lux/status koneksi router. Alamat LCD bawaan 0x27, bisa diubah ke 0x3F di `hardware_config.h` jika sesuai backpack milikmu.

## 4. Lihat pembacaan dari HP

1. Sambungkan HP ke hotspot `SmartGrow-Test-XXXXXX`, gunakan password yang tampil di Serial Monitor.
2. Tetap gunakan jaringan tersebut saat HP memberi pesan **“Tidak ada internet”**. Jika HP otomatis kembali ke data seluler/Wi-Fi lain, pilih tetap terhubung.
3. Buka browser biasa dan ketik **http://192.168.4.1**. Gunakan HTTP, bukan HTTPS.
4. Halaman menampilkan pembacaan sensor, hasil deteksi saat boot, dan tombol tes relay. Pembacaan diperbarui sekitar setiap 2,5 detik.

Wi-Fi rumah tidak wajib untuk langkah ini. Semua tampilan dilayani langsung oleh ESP32, tanpa CDN atau server luar. Jika koneksi ke alat putus, halaman memberi peringatan bahwa nilai mungkin sudah lama dan menonaktifkan tombol sampai terhubung lagi.

## 5. Opsional: coba Wi-Fi rumah

Pada halaman lokal, masukkan SSID jaringan **2,4 GHz** dan password. Alat mencoba sampai 60 detik. Jika mendapat IP dan kredensial berhasil disimpan, status menjadi berhasil. Jika gagal, kredensial sebelumnya dipertahankan dan hotspot uji tetap tersedia.

Ini hanya menguji sambungan ke router dan penerimaan alamat IP, bukan ketersediaan internet atau MQTT. Mode uji tidak menghubungi broker/NTP. Jaringan terbuka, WPA-Enterprise, dan login captive portal belum didukung; password 8–63 byte.

Saat ESP32 mengubah kanal untuk mengikuti router, HP dapat terputus sebentar. Sambungkan kembali ke hotspot SmartGrow lalu buka alamat yang sama. Dalam mode uji, halaman tetap diakses melalui hotspot alat, bukan melalui IP router yang ditampilkan.

Password Wi-Fi rumah tidak dicetak ke Serial Monitor. Kredensial jaringan mode uji disimpan terpisah dari mode produksi. Ketika nanti pindah ke `esp32dev`, atur Wi-Fi produksi kembali melalui hotspot setup-nya.

## 6. Kalibrasi dan tes relay

Untuk kalibrasi soil moisture, catat `soil_raw` pada kondisi kering dan basah yang representatif. Ubah `SOIL_DRY_RAW`, `SOIL_WET_RAW`, dan `SOIL_CALIBRATED=true` di `firmware/include/hardware_config.h`, lalu upload ulang. Gunakan catu dan posisi pemasangan yang sama. Persentase ini relatif terhadap kalibrasi, bukan otomatis kadar air volumetrik ilmiah.

Relay ON tetap terkunci melalui `ENABLE_PUMP=false` dan `ENABLE_LAMP=false`. Untuk memeriksa sensor, biarkan nilai tersebut. Setelah polaritas modul dan rangkaian diuji tanpa beban, sesuaikan `PUMP_ACTIVE_LOW`/`LAMP_ACTIVE_LOW` lalu aktifkan hanya keluaran yang ingin diuji. Upload ulang environment `esp32-local`.

Tombol **Tes 2 detik** akan mengaktifkan keluaran lalu mematikannya melalui timer terpisah, termasuk saat koneksi HP terputus. Tombol **Matikan** tersedia untuk memutus lebih cepat. Uji dengan indikator/multimeter terlebih dahulu; jangan menyimpulkan polaritas benar hanya karena halaman menampilkan OFF. Untuk lampu yang benar-benar UV, pastikan jenis lampu dan pelindungnya sebelum pengujian berbeban.

## 7. Jika ada kendala

| Gejala | Langkah berikutnya |
|---|---|
| Port COM tidak muncul | Coba kabel USB data/port USB lain. Identifikasi chip USB-to-serial pada board; pasang driver dari produsen yang sesuai bila diperlukan. |
| Upload berhenti pada `Connecting...` | Tahan BOOT, tekan dan lepaskan EN sebentar, lalu lepaskan BOOT setelah proses penulisan mulai. Tutup Serial Monitor/aplikasi lain yang memakai port. |
| Serial Monitor kosong | Periksa port dan baud 115200, lalu tekan EN/RESET. Jika mencetak pesan meminta `device_config.h`, kamu mengunggah environment `esp32dev`; pilih `esp32-local`. |
| LCD kosong | Periksa catu, kabel SDA21/SCL22, alamat 0x27/0x3F, dan potensiometer kontras. Pastikan pull-up backpack tidak menarik GPIO ESP32 ke 5 V. |
| DHT11 tidak terbaca | Periksa urutan pin modul, ground, catu, serta pull-up DATA ke 3,3 V. GPIO5 dipertahankan sesuai rancangan; periksa perilaku cold boot karena merupakan strapping pin. |
| Probe suhu tidak terbaca | Periksa DQ GPIO4 dan pull-up sekitar 4,7 kΩ ke 3,3 V; gunakan catu tiga kabel. Pastikan model fisik sensor dan urutan kabelnya. |
| BH1750 tidak terbaca | Periksa SDA21/SCL22, catu modul, dan alamat bawaan 0x23. Jika belum terpasang, angka kosong memang diharapkan. |
| Sensor baru dipasang belum dikenali | Matikan alat sebelum mengubah wiring, lalu nyalakan kembali. Deteksi awal probe/LCD/BH1750 dilakukan saat boot. |
| Hotspot ada tetapi halaman tidak terbuka | Pastikan HP tetap di hotspot alat, gunakan `http://192.168.4.1`, lalu muat ulang setelah perubahan Wi-Fi. |

## Setelah tes lokal berhasil

Siapkan VPS mengikuti `README.md`, buat identitas unik setiap alat, lalu pilih environment **esp32dev** ketika build/upload. Mode uji bukan pengganti autentikasi akun atau kendali jarak jauh produksi.

Referensi: https://docs.platformio.org/en/latest/integration/ide/vscode.html dan https://docs.espressif.com/projects/esptool/en/latest/esp32/advanced-topics/boot-mode-selection.html .
