# Upload source SmartGrow ke GitHub

Paket ZIP ini berisi seluruh source SmartGrow 0.1.1. Firmware utama ada di `firmware/src/main.cpp`; frontend di `backend/templates/` dan `backend/static/`; backend/API di `backend/garden/`; serta konfigurasi VPS di `compose.yaml`, `deploy/`, dan `tools/`.

## Upload ke repo baru yang kosong

1. Ekstrak ZIP ke folder biasa. Buka folder **SmartGrow** pada VS Code. Pastikan `README.md`, `.gitignore`, `.env.example`, dan `compose.yaml` berada langsung di folder tersebut.
2. Buat repository baru bernama `smartgrow` di GitHub. Untuk tahap pengembangan produk, kamu dapat memilih **Private**. Biarkan repo kosong: jangan membuat README, `.gitignore`, atau license tambahan saat pembuatan karena paket sudah memiliki README dan `.gitignore`.
3. Pastikan Git sudah terpasang dan kamu dapat melakukan autentikasi ke GitHub. Buka terminal VS Code pada folder SmartGrow, lalu jalankan:

```bash
git init
git add .
git status
git commit -m "Initial SmartGrow source"
git branch -M main
git remote add origin https://github.com/USERNAME/smartgrow.git
git push -u origin main
```

Ganti `USERNAME` dengan username pemilik repository, atau salin URL HTTPS repo dari GitHub dan gunakan sebagai alamat `origin`. Tinjau daftar file pada `git status` sebelum commit. Jika Git meminta identitas pembuat commit, atur nama/email Git milikmu, lalu ulangi commit yang belum berhasil. Ikuti autentikasi GitHub pada perangkatmu; jangan menaruh token/password dalam URL repo atau source code.

Jika memakai GitHub Desktop, tambahkan folder SmartGrow sebagai local repository, commit perubahan, lalu publish ke repository baru dengan pengaturan akses yang kamu pilih. Jangan membuat repo kedua jika repo tujuan sudah ada; clone repo tersebut dan masukkan source ke dalam clone yang benar.

## File yang masuk repository

- Source firmware, backend, frontend, tests, migrations, konfigurasi contoh, dan dokumentasi.
- `.env.example` serta `firmware/include/device_config.example.h`: contoh konfigurasi yang tidak berisi identitas produksi.
- `.gitignore`: ikut commit agar hasil build, database lokal, dan rahasia yang dibuat kemudian tidak ikut terlacak.

## File privat dan hasil build

`.gitignore` mengecualikan `.env` dan variasinya selain `.env.example`, `.private/`, header `firmware/include/device_config.h`, password/ACL broker hasil provisioning, sertifikat server, database SQLite, hasil build PlatformIO, log, serta backup bernama `smartgrow-backup*.sql` atau `smartgrow-backup*.dump`.

Nama file backup lain juga perlu dikecualikan bila kamu membuatnya. Aturan `.gitignore` bekerja pada Git; pengunggahan file langsung melalui browser tetap perlu dipilih dengan teliti. File yang sudah terlanjur dilacak Git tidak otomatis hilang dari riwayat hanya karena ditambahkan ke `.gitignore`.

Paket ini belum dipush ke GitHub secara otomatis. Repository tujuan serta aksesnya ditentukan oleh pemilik proyek. Dokumentasi validasi perangkat lunak dan pengujian fisik yang belum dilakukan tetap ada di `VALIDATION.md`.

Referensi resmi: https://docs.github.com/en/migrations/importing-source-code/using-the-command-line-to-import-source-code/adding-locally-hosted-code-to-github
