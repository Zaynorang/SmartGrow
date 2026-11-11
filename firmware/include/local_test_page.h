#pragma once
static const char LOCAL_TEST_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="id">
<head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="referrer" content="no-referrer"><title>SmartGrow · Uji perangkat</title>
<style>
:root{font-family:system-ui,-apple-system,sans-serif;color:#193e32;background:#f5f6ef;color-scheme:light}*{box-sizing:border-box}body{margin:0}header{background:white;border-bottom:1px solid #dfe5dc;padding:20px max(20px,calc((100vw - 1060px)/2));display:flex;justify-content:space-between;align-items:center;gap:14px}header strong{font-size:25px;letter-spacing:-1px}.tag{font-size:12px;font-weight:700;background:#e8efdc;padding:8px 12px;border-radius:30px;white-space:nowrap}main{max-width:1100px;padding:32px 20px 50px;margin:auto}h1{font-size:clamp(29px,5vw,42px);letter-spacing:-1.3px;line-height:1.15;margin:12px 0}h2{font-size:22px;margin:7px 0 14px}p{line-height:1.6;margin:10px 0}.muted,small{color:#637167}small{font-size:13px;line-height:1.6}.eyebrow{font-size:11px;letter-spacing:1.5px;font-weight:750;text-transform:uppercase}.grid{display:grid;grid-template-columns:repeat(3,1fr);gap:14px;margin:26px 0}.card,.panel{background:white;border:1px solid #dfe5dc;border-radius:15px;padding:22px}.card label{font-size:13px;color:#637167}.value{font-size:30px;font-weight:750;margin:12px 0;min-height:40px}.layout{display:grid;grid-template-columns:1fr 1fr;gap:18px}.panel{min-width:0}.banner{background:#edf2e3;border-radius:10px;padding:13px 16px}.error{background:#fcebe7;color:#8d3427}label{display:block}input,button{font:inherit;border-radius:8px;padding:11px 13px;border:1px solid #cbd6c9}input{width:100%;margin:7px 0 15px}button{background:#235b43;color:white;cursor:pointer;font-weight:650}button.secondary{background:white;color:#235b43}button:disabled{opacity:.45;cursor:not-allowed}.buttons{display:flex;gap:9px;flex-wrap:wrap;margin:12px 0}.relay{border-top:1px solid #e4e9df;padding:17px 0 2px}.relay-head{display:flex;justify-content:space-between;align-items:center;gap:10px}.state{font-size:12px;background:#f1f3ed;border-radius:20px;padding:5px 10px;font-weight:700}.good{color:#205f37}.bad{color:#9a3b29}code{font-size:12px;overflow-wrap:anywhere}#notice{min-height:24px}footer{margin-top:24px}a{color:#235b43}button:focus-visible,input:focus-visible{outline:3px solid #99be6c;outline-offset:3px}@media(max-width:700px){.grid{grid-template-columns:repeat(2,1fr);gap:10px}.layout{grid-template-columns:1fr}.card,.panel{padding:17px}.value{font-size:26px}header strong{font-size:23px}}@media(max-width:370px){.grid{grid-template-columns:1fr}header{flex-wrap:wrap}}
</style></head>
<body>
<header><strong>smartgrow</strong><span class="tag">UJI LOKAL · %DEVICE%</span></header>
<main>
<div class="eyebrow">Kenali kondisi perangkatmu</div><h1>Tes sensor, langsung dari ESP32.</h1>
<p class="muted">Halaman ini bekerja melalui hotspot alat. VPS, akun, dan internet belum diperlukan.</p>
<p id="connection" class="banner" role="status">Menunggu pembacaan perangkat…</p>
<section class="grid" aria-label="Pembacaan sensor">
<article class="card"><label>Tanah · nilai mentah</label><div class="value" id="soil_raw">—</div><small>GPIO34 · ADC 0–4095</small></article>
<article class="card"><label>Kelembapan tanah</label><div class="value" id="soil_pct">—</div><small id="calibration">Belum dikalibrasi</small></article>
<article class="card"><label>Suhu udara</label><div class="value" id="air_c">—</div><small>DHT11 · GPIO5</small></article>
<article class="card"><label>Kelembapan udara</label><div class="value" id="humidity_pct">—</div><small>DHT11 · GPIO5</small></article>
<article class="card"><label>Suhu probe</label><div class="value" id="probe_c">—</div><small>DS18x20 · GPIO4</small></article>
<article class="card"><label>Cahaya sekitar</label><div class="value" id="lux">—</div><small>BH1750 · SDA21 / SCL22</small></article>
</section>
<div class="layout">
<section class="panel"><div class="eyebrow">Pemeriksaan perangkat</div><h2>Sensor &amp; relay</h2>
<p id="discovery" class="muted">Menunggu hasil deteksi saat boot.</p>
<small>Tanda — berarti belum ada pembacaan valid. Nilai ADC tetap perlu diperiksa meskipun probe tanah belum terpasang. Persentase tanah baru muncul setelah kalibrasi.</small>
<div class="relay"><div class="relay-head"><strong>Pompa · GPIO14</strong><span id="pump-state" class="state">—</span></div><div class="buttons"><button data-target="pump" data-action="on" disabled>Tes 2 detik</button><button class="secondary" data-target="pump" data-action="off" disabled>Matikan</button></div><small id="pump-lock">Menunggu perangkat.</small></div>
<div class="relay"><div class="relay-head"><strong>Lampu · GPIO26</strong><span id="lamp-state" class="state">—</span></div><div class="buttons"><button data-target="lamp" data-action="on" disabled>Tes 2 detik</button><button class="secondary" data-target="lamp" data-action="off" disabled>Matikan</button></div><small id="lamp-lock">Menunggu perangkat.</small></div>
<p id="notice" role="status" aria-live="polite"></p><small>Uji indikator relay dengan beban pompa/lampu terlepas. Status di sini adalah perintah keluaran, bukan pengukuran aliran air atau cahaya lampu.</small>
</section>
<section class="panel"><div class="eyebrow">Opsional</div><h2>Coba Wi-Fi rumah</h2>
<p id="wifi-state" class="muted">Sensor dapat diuji tanpa mengisi Wi-Fi rumah.</p>
<form id="wifi-form" method="post" action="/save"><input type="hidden" name="token" value="%TOKEN%">
<label for="ssid">Nama Wi-Fi 2,4 GHz</label><input id="ssid" name="ssid" list="networks" maxlength="32" required autocomplete="off"><datalist id="networks"></datalist>
<label for="password">Kata sandi Wi-Fi</label><input id="password" name="password" type="password" minlength="8" maxlength="63" required autocomplete="new-password">
<button id="wifi-save" disabled>Coba sambungkan</button></form><p id="wifi-result" role="status" aria-live="polite"></p>
<small>Hotspot uji tetap aktif. Saat alat berpindah kanal Wi-Fi, HP mungkin perlu disambungkan ulang ke hotspot. Koneksi berhasil berarti alat mendapat IP dari router; akses internet tidak diuji.</small>
<p><small id="scan">Memindai nama Wi-Fi…</small></p></section>
</div>
<footer><small id="device-info">SmartGrow · mode uji lokal</small><p><small>LCD menampilkan alamat halaman dan sensor secara bergantian. Hasil deteksi perangkat diperiksa saat boot; setelah mengubah kabel, matikan dan nyalakan alat kembali.</small></p></footer>
</main>
<script>
'use strict';
const $=id=>document.getElementById(id), token=document.querySelector('[name=token]').value;
let fresh=false,busy=false,wifiBusy=false;
const metrics={soil_raw:['',0],soil_pct:[' %',1],air_c:[' °C',1],humidity_pct:[' %',1],probe_c:[' °C',1],lux:[' lux',0]};
function value(v,unit,digits){return typeof v==='number'&&Number.isFinite(v)?v.toLocaleString('id-ID',{maximumFractionDigits:digits})+unit:'—';}
function lockAll(){document.querySelectorAll('button').forEach(b=>b.disabled=true);}
function render(d){
 fresh=true;
 for(const [key,[unit,digits]] of Object.entries(metrics)) $(key).textContent=value(d[key],unit,digits);
 $('calibration').textContent=d.soil_calibrated?'Hasil kalibrasi relatif':'Belum dikalibrasi · lihat panduan';
 $('connection').className='banner';$('connection').textContent='Terhubung ke ESP32 · diperbarui '+new Date().toLocaleTimeString('id-ID');
 $('discovery').textContent='Deteksi saat boot: LCD '+(d.lcd_found?'ada':'tidak terdeteksi')+' · BH1750 '+(d.bh1750_found?'ada':'tidak terdeteksi')+' · probe suhu '+(d.probe_found?'ada':'tidak terdeteksi')+'.';
 for(const target of ['pump','lamp']){
  $(target+'-state').textContent=d[target]?'ON':'OFF';
  $(target+'-lock').textContent=d[target+'_enabled']?'Tes diizinkan. Output berhenti setelah 2 detik.':'Terkunci. Periksa polaritas, lalu aktifkan di hardware_config.h.';
  document.querySelectorAll('[data-target="'+target+'"]').forEach(b=>b.disabled=busy||(b.dataset.action==='on'&&(!d[target+'_enabled']||d.wifi_phase==='trying')));
 }
 $('wifi-save').disabled=wifiBusy||d.wifi_phase==='trying';
 $('wifi-state').textContent=d.wifi_connected?'Tersambung ke '+d.wifi_ssid+' · IP router '+d.wifi_ip:'Belum tersambung ke router. Halaman lokal tetap bekerja.';
 const phases={trying:'Sedang mencoba jaringan. Tunggu hingga 60 detik.',saved:'Wi-Fi berhasil tersambung dan disimpan.',failed:'Percobaan gagal atau penyimpanan gagal. Konfigurasi lama dipertahankan.'};
 if(phases[d.wifi_phase])$('wifi-result').textContent=phases[d.wifi_phase];
 $('device-info').textContent='SmartGrow '+d.firmware+' · perangkat '+d.device+' · aktif '+Math.floor(d.uptime_s)+' detik';
}
async function request(url,options={}){
 const controller=new AbortController(),timer=setTimeout(()=>controller.abort(),7000);
 try{return await fetch(url,{...options,cache:'no-store',signal:controller.signal});}finally{clearTimeout(timer);}
}
async function update(){
 try{const r=await request('/data');if(!r.ok)throw new Error('data');render(await r.json());}
 catch(e){fresh=false;lockAll();$('connection').className='banner error';$('connection').textContent='Koneksi ke alat terputus. Nilai di bawah mungkin sudah lama. Sambungkan HP kembali ke hotspot SmartGrow.';}
}
async function poll(){await update();setTimeout(poll,2500);}
document.querySelectorAll('[data-target]').forEach(button=>button.addEventListener('click',async()=>{
 if(!fresh||busy)return;busy=true;document.querySelectorAll('[data-target]').forEach(b=>b.disabled=true);
 try{const r=await request('/relay',{method:'POST',body:new URLSearchParams({token,target:button.dataset.target,action:button.dataset.action})});$('notice').textContent=await r.text();}
 catch(e){$('notice').textContent='Belum ada konfirmasi perintah. Periksa indikator alat; jangan menganggap perintah gagal terkirim.';}
 finally{busy=false;await update();}
}));
$('wifi-form').addEventListener('submit',async event=>{
 event.preventDefault();if(!fresh||wifiBusy)return;wifiBusy=true;$('wifi-save').disabled=true;
 try{const r=await request('/save',{method:'POST',body:new URLSearchParams(new FormData(event.target))});$('wifi-result').textContent=await r.text();if(r.ok)$('password').value='';}
 catch(e){$('wifi-result').textContent='Konfirmasi belum diterima. Periksa koneksi hotspot; tunggu satu menit sebelum mencoba lagi.';}
 finally{wifiBusy=false;await update();}
});
async function scan(){
 try{const r=await request('/networks');if(!r.ok)throw new Error('scan');const d=await r.json();if(d.scanning){setTimeout(scan,3000);return;}
  $('networks').replaceChildren();for(const ssid of d.ssids){const o=document.createElement('option');o.value=ssid;$('networks').append(o);}
  $('scan').textContent='Nama Wi-Fi juga dapat diketik secara manual.';
 }catch(e){$('scan').textContent='Pemindaian belum tersedia. Ketik nama Wi-Fi secara manual.';}
}
poll();scan();
</script></body></html>
)HTML";
