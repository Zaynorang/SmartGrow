#pragma once
static const char PORTAL_HTML[] PROGMEM = R"HTML(
<!doctype html><html lang="id"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>SmartGrow · Pengaturan Wi-Fi</title><style>body{font:16px system-ui;background:#f3f6ed;color:#183e32;margin:0;padding:26px}main{max-width:420px;margin:25px auto;background:white;border-radius:20px;padding:28px}h1{font-size:29px}p{line-height:1.6;color:#647567}label{display:block;margin:20px 0}input,button{box-sizing:border-box;width:100%;padding:13px;border:1px solid #c9d7c4;border-radius:9px;font:inherit;margin-top:7px}button{background:#246347;color:white}small{color:#6b7866}</style>
<main><b>smartgrow</b><h1>Hubungkan kebun Anda.</h1><p>Ketik nama Wi-Fi rumah 2,4 GHz (SSID) dan masukkan kata sandinya.</p>
<form method="post" action="/save"><input type="hidden" name="token" value="%TOKEN%"><label>Nama Wi-Fi (SSID)<input name="ssid" type="text" maxlength="32" required autocomplete="off" autocapitalize="none" spellcheck="false"></label><label>Kata sandi<input name="password" type="password" minlength="8" maxlength="63" required autocomplete="new-password"></label><button>Simpan dan sambungkan</button></form><small>%DEVICE% · Pengaturan tersimpan pada alat.</small></main>
</html>
)HTML";
