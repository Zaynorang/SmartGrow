# SmartGrow 0.1.1 — paket awal ESP32 + web + MQTT

**Belum punya VPS dan ingin mengetes ESP32 dulu? Baca `MULAI-DI-SINI-TES-ESP32.md`.** Environment bawaan sekarang `esp32-local`: sensor, LCD, hotspot, halaman uji lokal, dan Serial Monitor dapat berjalan tanpa server atau `device_config.h`. Mode server tetap tersedia melalui environment `esp32dev`.

Paket ini merupakan implementasi awal yang dapat dikembangkan, bukan firmware yang sudah diuji pada alat fisik pengguna. Pin mengikuti percakapan: soil GPIO34, DHT11 GPIO5, DS1820 GPIO4, LCD I²C GPIO21/22, relay pompa GPIO14; tambahan BH1750 pada I²C yang sama dan relay lampu GPIO26.

**Mulai dengan membaca panduan ini. Jangan langsung mengunggah firmware ke alat yang masih membawa program lama dan beban pompa/lampu tersambung. Cadangkan firmware lama, cek polaritas modul relay, lalu uji tanpa beban terlebih dahulu.**

## Isi paket

| Folder/file | Fungsi |
|---|---|
| `firmware/src/main.cpp` | Firmware ESP32 C++ dengan portal Wi-Fi, MQTT TLS, sensor, LCD, dan relay |
| `firmware/include/hardware_config.h` | Pin, alamat LCD, polaritas relay, batas durasi, dan kalibrasi |
| `firmware/platformio.ini` | Dependensi dan profil kompilasi ESP32 DevKit klasik |
| `backend/` | Django: akun, aktivasi perangkat, dashboard, histori, kontrol, perubahan Wi-Fi |
| `backend/templates/` dan `backend/static/` | Frontend dashboard: HTML, CSS, JavaScript, grafik, dan form kontrol |
| `compose.yaml` | PostgreSQL, Django, worker MQTT, Mosquitto, nginx, Certbot |
| `tools/deploy.py` | Persiapan VPS, sertifikat HTTPS/MQTT, pembaruan sertifikat |
| `tools/provision_device.py` | Identitas MQTT, kode aktivasi, kartu pembeli, dan header ESP32 per alat |
| `docs/PROTOCOL.md` | Kontrak topik, payload, konfirmasi, dan status |
| `docs/VALIDATION.md` | Hasil verifikasi dan batas pengujian |
| `docs/GITHUB.md` | Panduan memasukkan seluruh source ke satu repository GitHub |
| `MULAI-DI-SINI-TES-ESP32.md` | Panduan upload dan tes lokal sebelum memiliki VPS |
| `firmware/include/local_test_page.h` | Halaman uji langsung di hotspot ESP32, hanya untuk `esp32-local` |

## Source lengkap dan GitHub

Seluruh isi folder ini merupakan source lengkap SmartGrow versi awal: firmware C++, frontend HTML/CSS/JavaScript, backend Django, konfigurasi server, serta panduan pengujian. Jadikan isi folder **SmartGrow** sebagai root repository, sehingga `README.md`, `.gitignore`, `.env.example`, `compose.yaml`, `firmware/`, dan `backend/` langsung terlihat di halaman utama repo. Ikuti `docs/GITHUB.md` untuk perintah upload.

Library eksternal tidak dibundel: PlatformIO memasangnya dari `firmware/platformio.ini`, sedangkan pip memasangnya dari `backend/requirements.txt`. File server/perangkat berisi rahasia dibuat saat deployment/provisioning dan tidak disertakan dalam paket source.

## Fitur yang sudah diimplementasikan

- Registrasi dan login akun, logout melalui POST, perlindungan CSRF, serta pembatasan percobaan login/aktivasi.
- Satu akun dapat memiliki beberapa perangkat; setiap permintaan detail/kontrol memeriksa pemiliknya.
- Kode aktivasi acak sekali pakai, disimpan sebagai hash dan dihapus setelah klaim berhasil.
- Kredensial MQTT unik per alat dan ACL topik yang membatasi komunikasi tiap alat.
- Pembacaan sensor, status relay, status online, dan grafik satu jam; data disimpan 30 hari.
- Data sensor dikirim setiap 15 detik; dashboard memperbarui setiap 5 detik. Alat dianggap offline setelah 45 detik tanpa telemetri atau setelah pesan offline diterima.
- Kontrol manual pompa ON berjangka 1–60 detik dan lampu ON berjangka 1–900 detik, serta OFF manual. Tidak ada mode ON tanpa batas.
- Relay kembali ke OFF logis pada boot. Timer pemutus berjalan di task FreeRTOS terpisah dari jaringan.
- Hotspot konfigurasi otomatis saat belum ada Wi-Fi; setelah koneksi Wi-Fi hilang, retry lalu hotspot aktif setelah 60 detik. Portal memiliki kata sandi unik, token formulir, dan akses dari subnet hotspot.
- Ganti Wi-Fi dari web saat alat online; ESP32 mencoba kredensial kandidat dan mempertahankan konfigurasi lama jika gagal.
- Penggantian dari web baru dikomit setelah MQTT TLS berhasil di jaringan baru. Pengaturan lokal pertama dapat disimpan setelah mendapat IP, walaupun internet sedang tidak tersedia.
- Putus internet/MQTT saja tidak menghapus Wi-Fi atau mengaktifkan hotspot jika asosiasi ke router masih ada.
- Kata sandi Wi-Fi baru tidak dimasukkan database, riwayat perintah, atau pesan retained. HTTPS dan MQTT TLS melindungi pengirimannya.

**Belum termasuk:** penyiraman berdasarkan ambang otomatis, jadwal lampu, pengaturan intensitas lampu, sensor aliran/ketinggian air, firmware OTA, reset password melalui email, alur pindah kepemilikan, atau aplikasi Android/iOS. Semua ini dapat ditambahkan setelah versi dasar lulus pengujian perangkat. Nama “UV” belum dikonfirmasi; kendali lampu dikunci secara bawaan.

## 1. Periksa perangkat keras

Asumsi board: ESP32 klasik DevKit/WROOM dengan flash 4 MB, bukan ESP32-C3/S3. Label Dxx diasumsikan GPIOxx.

| Perangkat | Koneksi data | Pemeriksaan |
|---|---|---|
| Soil moisture | AO → GPIO34 | Tegangan AO harus sesuai input ESP32; jangan masukkan 5 V ke GPIO. Persentase perlu kalibrasi. |
| DHT11 | DATA → GPIO5 | Tetap mengikuti keinginan pengguna. Periksa pull-up DATA ke level 3,3 V; GPIO5 adalah strapping pin. Uji beberapa cold boot. |
| DS1820 / keluarga DS18x20 | DQ → GPIO4 | Komunikasi digital 1-Wire. Gunakan catu tiga kabel, bukan parasite-power, dengan pull-up DQ sekitar 4,7 kΩ ke 3,3 V bila belum ada pada modul. Pastikan label model fisiknya. |
| LCD 1602 I²C | SDA → GPIO21, SCL → GPIO22 | Alamat bawaan 0x27, bisa perlu 0x3F. Backpack dengan pull-up 5 V memerlukan penyesuaian level I²C. |
| BH1750 | SDA → GPIO21, SCL → GPIO22 | Alamat bawaan 0x23. Ukur lux cahaya tampak, bukan UV atau PPFD. Sesuaikan catu dengan modul. |
| Modul relay pompa | IN → GPIO14 | Bawaan kode mengasumsikan active-low; cek modul sebenarnya. |
| Modul relay lampu | IN → GPIO26 | Cek jenis lampu, tegangan, arus, dan polaritas modul relay. |

ESP32 mengendalikan input modul relay/driver, bukan kumparan relay atau pompa langsung. Catu beban dan proteksi harus sesuai perangkat. GPIO saat boot belum dapat dijamin OFF oleh perangkat lunak saja; rangkaian pull-up/pull-down/driver perlu memastikan keadaan aman sejak listrik masuk.

Di `hardware_config.h`, nilai bawaan adalah `ENABLE_PUMP=false` dan `ENABLE_LAMP=false`. Ini menolak perintah ON dalam aplikasi; level OFF fisik tetap bergantung pada `PUMP_ACTIVE_LOW`/`LAMP_ACTIVE_LOW` yang benar. Setelah diuji tanpa beban, ubah hanya keluaran yang sudah diverifikasi menjadi `true`, lalu kompilasi dan unggah ulang.

Kalibrasi tanah: baca `soil_raw` dari dashboard (pilih grafik Tanah mentah), ukur kondisi kering dan kondisi basah yang representatif dengan pemasangan sensor yang sama. Isikan `SOIL_DRY_RAW`/`SOIL_WET_RAW`, kemudian `SOIL_CALIBRATED=true`. Nilai persen adalah kelembapan relatif hasil kalibrasi, bukan otomatis kadar air volumetrik ilmiah. Sebelum kalibrasi, `soil_pct` dikirim `null` dan dashboard tetap menampilkan nilai mentah.

Sensor opsional yang belum terpasang: set `USE_BH1750=false` dan/atau `USE_LCD=false`. Sensor yang gagal dibaca dilaporkan kosong, bukan nol palsu. DallasTemperature mengenali keluarga DS18S20/DS1820 (family 0x10) dan DS18B20; satu probe pertama pada bus digunakan.

## 2. Siapkan VPS dan domain

Gunakan VPS Linux dengan Docker Engine, plugin Docker Compose v2, dan Python 3. Instruksi berikut mengasumsikan akses menjalankan Docker sudah tersedia. Instal Docker dari dokumentasi resminya: https://docs.docker.com/engine/install/ .

1. Salin seluruh folder SmartGrow ke VPS, misalnya `/opt/smartgrow`.
2. Buat DNS A/AAAA yang benar untuk dua nama, misalnya `garden.domainanda.id` dan `mqtt.domainanda.id`, menuju VPS. Jangan memasang AAAA jika VPS tidak memiliki IPv6 yang bekerja. Untuk MQTT, gunakan DNS langsung (bukan proxy HTTP/CDN biasa).
3. Pastikan port TCP **80, 443, 8883** tersedia dan diizinkan firewall penyedia VPS. Jangan membuka port database 5432, Django 8000, atau MQTT 1883 ke internet. Container 1883 hanya digunakan di jaringan Compose.
4. Jalankan dari folder proyek; ganti nama domain dan alamat email dengan milikmu:

```bash
python3 tools/deploy.py init --web-domain garden.domainanda.id --mqtt-domain mqtt.domainanda.id --email admin@domainanda.id
python3 tools/deploy.py tls
docker compose exec web python manage.py createsuperuser
```

`init` membuat rahasia acak, konfigurasi autentikasi MQTT, database, dan nginx HTTP sementara. `tls` meminta satu sertifikat Let's Encrypt untuk kedua domain, memasang HTTPS, lalu memulai Mosquitto dan worker. Perintah Certbot mengirim domain/email ke Let's Encrypt dan menyetujui ketentuan penerbitan sertifikat saat kamu menjalankannya. Situs kemudian tersedia melalui domain web milikmu. Tidak ada akun admin/password bawaan.

Jika `init` berhenti setelah membuat `.env`, jangan hapus file tersebut atau membuat kredensial baru. Periksa pesan kesalahan, kemudian lanjutkan langkah yang belum berhasil:

```bash
docker compose build
docker compose up -d db
docker compose run --rm web python manage.py migrate --noinput
docker compose up -d web nginx
python3 tools/deploy.py tls
```

Jika nama domain sertifikat sudah benar tetapi penerbitan gagal, periksa DNS, firewall 80, dan port yang dipakai layanan lain. Hindari berulang kali meminta sertifikat sebelum memperbaiki penyebabnya.

Sertifikat harus diperbarui. Jalankan `python3 tools/deploy.py renew` melalui cron dua kali sehari menggunakan akun yang sama dan berhak menjalankan Docker. Contoh jika proyek benar-benar berada di `/opt/smartgrow`:

```cron
17 2,14 * * * cd /opt/smartgrow && /usr/bin/python3 tools/deploy.py renew >> /opt/smartgrow/cert-renew.log 2>&1
```

Periksa log pembaruan dan masa berlaku sertifikat. Jangan menempelkan isi `.env`, header per perangkat, atau kartu aktivasi ke repositori publik.

## 3. Daftarkan satu perangkat di server

Jalankan **di VPS** setelah layanan berjalan:

```bash
python3 tools/provision_device.py SG0001 --name "Kebun Rumah"
```

ID harus unik, terdiri dari 3–16 huruf/angka/underscore/tanda hubung. Skrip menghasilkan:

- `.private/SG0001/device_config.h`: kredensial MQTT, domain broker, kata sandi hotspot, dan root CA. Salin ke komputer pengembang pada `firmware/include/device_config.h` sebelum kompilasi. Header ini untuk pengembang, jangan dibagikan ke pembeli.
- `.private/SG0001/KARTU-PERANGKAT.txt`: ID, kode aktivasi sekali pakai, kata sandi hotspot, serta alamat dashboard. Kartu ini diserahkan secara privat kepada pemilik alat.
- `.private/SG0001/registration.json`: salinan pemulihan pendaftaran; simpan privat.

Skrip menggunakan hash password buatan utilitas resmi Mosquitto, bukan algoritma buatan sendiri. ACL membatasi alat pada topiknya sendiri. Hanya backend yang dapat mengirim perintah. Saat memakai CA selain Let's Encrypt ISRG Root X1, tambahkan `--ca-file /path/root-ca.pem` yang sesuai; jangan menonaktifkan validasi sertifikat.

Jika pendaftaran terputus setelah file privat terbentuk, jangan menjalankan ulang dengan ID sama untuk membuat identitas baru. Periksa bahwa ACL/password broker sudah ditambahkan dan bahwa ID belum ada di admin. Jika broker sudah siap tetapi pencatatan database belum dibuat, lanjutkan dengan identitas yang sudah disimpan:

```bash
docker compose exec -T web python manage.py create_device < .private/SG0001/registration.json
docker compose kill -s HUP mosquitto
```

Perintah `create_device` menolak ID yang sudah ada. Jika kegagalan terjadi sebelum ACL/password ditulis, perbaikan administrator diperlukan menggunakan header privat yang sama; jangan menebak atau mengganti identitas perangkat yang sudah beredar.

## 4. Kompilasi dan unggah ESP32

Bagian ini untuk **mode server `esp32dev`**, setelah VPS dan identitas perangkat siap. Untuk tes tanpa server, gunakan panduan uji lokal di awal paket.

Di komputer Windows, cara yang disiapkan paket ini adalah **VS Code + PlatformIO** (C++ Arduino framework). Dependensi dipatok pada `platformio.ini`. Ini menghindari variasi library `LiquidCrystal_I2C` dari vendor berbeda.

1. Cadangkan firmware lama jika masih dibutuhkan.
2. Buka folder `firmware` sebagai proyek PlatformIO.
3. Salin `device_config.h` hasil langkah 3 ke `firmware/include/`.
4. Periksa `hardware_config.h`: pin, polaritas, alamat LCD, dan kalibrasi. Biarkan beban pompa/lampu terlepas untuk uji awal.
5. Pada **Project Tasks → esp32dev → General**, pilih **Build**, lalu **Upload**, lalu **Monitor** 115200 baud. Pilih environment ini secara eksplisit: tombol/default build biasa menggunakan `esp32-local`.

Alternatif terminal, setelah PlatformIO terpasang:

```bash
cd firmware
pio run -e esp32dev
pio run -e esp32dev --target upload --upload-port COM5
pio device monitor -e esp32dev --port COM5 --baud 115200
```

Ganti COM5 dengan port alat. Di Linux biasanya `/dev/ttyUSB0` atau `/dev/ttyACM0`. Profil menggunakan partisi `huge_app` tanpa OTA. Jika flash/board berbeda, ubah profil berdasarkan board sebenarnya.

Dalam environment `esp32dev`, tanpa `device_config.h`, proyek tetap dapat dikompilasi dengan contoh, tetapi tidak akan mencoba MQTT atau membuka hotspot berkredensial contoh. Dalam `esp32-local`, identitas MQTT tidak diperlukan: alat membuat hotspot uji dengan kata sandi acak per alat. Tidak ada binary siap unggah yang berisi identitas produksi bersama semua pembeli.

## 5. Alur pembeli

1. Nyalakan SmartGrow.
2. Sambungkan HP ke `SmartGrow-Setup-SG0001` dan masukkan kata sandi hotspot pada kartu. Keterangan “tanpa internet” di jaringan ini normal.
3. Buka `http://192.168.4.1` jika halaman tidak muncul otomatis. Pilih/ketik SSID 2,4 GHz dan kata sandi (8–63 byte); jaringan terbuka, WPA-Enterprise, dan captive portal hotel/kampus belum didukung.
4. Setelah tersambung, kembali ke internet normal di HP. Daftar/login di website SmartGrow.
5. Pilih **Tambah perangkat**; masukkan ID dan kode aktivasi dari kartu.
6. Tunggu data masuk. Tombol ON hanya aktif jika keluaran tersebut sudah diizinkan pada firmware dan alat online.

**Mengganti Wi-Fi dari dashboard:** saat alat online, isi SSID/kata sandi baru. Dashboard menunggu konfirmasi; menekan Simpan tidak berarti perubahan sudah berhasil. ESP32 mematikan relay secara logis sebelum beralih jaringan, menguji sampai 60 detik, lalu menyimpan atau mencoba kembali Wi-Fi lama. Jika kedua jaringan gagal, hotspot tersedia untuk pengaturan lokal.

Saat sambungan Wi-Fi yang sudah tersimpan hilang, alat mencoba ulang setiap 20 detik dan mengaktifkan hotspot setelah 60 detik. Saat internet/server saja putus tetapi router tetap terhubung, tidak ada reset Wi-Fi. Kontrol lokal berjangka yang sudah berjalan tetap diputus oleh timer. Jika koneksi baru/konfirmasi hilang saat reboot, website menampilkan “Belum ada konfirmasi akhir”; periksa status alat sebelum mengirim ulang.

## 6. Uji bertahap sebelum memasang beban

1. Nyalakan ulang ESP32 beberapa kali dan pastikan relay tidak aktif di luar perintah. Uji polaritas dengan indikator/multimeter tanpa beban pompa/lampu.
2. Pastikan semua sensor terbaca; lepas satu sensor dan periksa bahwa nilainya berubah menjadi kosong. Verifikasi sensor suhu memang model yang dimaksud.
3. Kalibrasi soil moisture; periksa alamat/level I²C LCD dan BH1750.
4. Buat dua akun uji dan dua identitas alat; pastikan setiap akun hanya melihat miliknya.
5. Aktifkan pompa 5 detik, putuskan internet, dan pastikan output tetap OFF setelah batas waktu.
6. Coba Wi-Fi baru benar, kata sandi salah, router mati, internet mati, VPS mati, dan restart broker. Periksa fallback tanpa menghapus konfigurasi yang masih valid.
7. Periksa kode aktivasi hanya berhasil sekali dan tidak dapat mengambil alih alat yang sudah terdaftar.
8. Jalankan uji operasi beberapa hari termasuk gangguan catu daya sebelum menyimpulkan layak dipakai pelanggan.

Untuk lampu yang benar-benar memancarkan UV, tentukan tipe/tujuan lampu dan rancangan pelindung/interlock sebelum mengaktifkan output. BH1750 tidak memvalidasi paparan UV. Durasi maksimum bawaan hanyalah batas program, bukan rekomendasi durasi penyinaran tanaman/manusia.

## 7. Operasi server

```bash
docker compose ps
docker compose logs --tail 100 web worker mosquitto
docker compose exec web python manage.py check --deploy
```

Satu VPS cukup sebagai arsitektur awal; kapasitas perlu diukur dari jumlah alat, frekuensi telemetri, dan retensi data. Dengan pengiriman 15 detik ada sekitar 5.760 baris per alat per hari. Untuk banyak alat, lakukan agregasi/downsampling dan ukur kapasitas database. Worker saat ini menghapus pembacaan lebih dari 30 hari.

Cadangkan PostgreSQL secara rutin di lokasi terpisah, bersama konfigurasi broker, konfigurasi server, sertifikat, serta data privat pendaftaran. Contoh dump dari host:

```bash
docker compose exec -T db pg_dump -U smartgrow smartgrow > smartgrow-backup.sql
```

File dump mengandung data pengguna; simpan privat. Pemulihan sebaiknya diuji pada lingkungan terpisah. Jangan menjalankan `docker compose down -v` pada instalasi aktif karena itu menghapus volume database.

Pembaruan kode backend (buat backup dahulu):

```bash
docker compose build web worker
docker compose run --rm web python manage.py migrate --noinput
docker compose up -d web worker
docker compose exec -u 0 web python manage.py collectstatic --noinput
docker compose exec nginx nginx -s reload
```

Rahasia Wi-Fi/perangkat berada dalam flash ESP32 tanpa enkripsi flash secara bawaan. Sebelum penjualan komersial, tentukan proses provisioning/manufaktur, secure boot/flash encryption jika diperlukan, pembaruan firmware, pencabutan perangkat, dukungan akun, dan uji kelistrikan. Tidak ada klaim bahwa hasil kompilasi atau tes backend menggantikan pengujian ini.

## 8. Pengembangan lokal dan tes

```bash
cd backend
python -m venv .venv
# Linux: source .venv/bin/activate
# PowerShell: .venv\Scripts\Activate.ps1
python -m pip install -r requirements.txt
```

Untuk sesi lokal saja, set `DEBUG=1`. Linux: `export DEBUG=1`; PowerShell: `$env:DEBUG='1'`. Lalu:

```bash
python manage.py migrate
python manage.py test garden -v 2
python manage.py runserver
```

SQLite dipakai jika `POSTGRES_HOST` tidak diisi. Versi VPS memakai PostgreSQL. Dashboard lokal tanpa broker/ESP32 akan menampilkan perangkat offline, bukan data simulasi. Jangan membuka server `DEBUG=1` ke internet.

Tes logika C++ tanpa ESP32, bila g++ tersedia:

```bash
g++ -std=c++11 -Wall -Wextra -Werror -I firmware/include firmware/tests/control_test.cpp -o control-test
./control-test
```

## Referensi teknis

- ESP32 Wi-Fi: https://docs.espressif.com/projects/arduino-esp32/en/latest/api/wifi.html
- GPIO ESP32: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/gpio.html
- Mosquitto konfigurasi: https://mosquitto.org/man/mosquitto-conf-5.html
- Mosquitto password utility: https://mosquitto.org/man/mosquitto_passwd-1.html
- Django auth: https://docs.djangoproject.com/en/5.2/topics/auth/
- Paho MQTT: https://eclipse.dev/paho/files/paho.mqtt.python/html/client.html
- DallasTemperature: https://github.com/milesburton/Arduino-Temperature-Control-Library
- BH1750: https://github.com/claws/BH1750
- Root CA Let's Encrypt: https://letsencrypt.org/certificates/

Dokumentasi ditulis untuk paket 0.1. Periksa pembaruan keamanan dependensi dan kompatibilitas board sebelum rilis berikutnya.
