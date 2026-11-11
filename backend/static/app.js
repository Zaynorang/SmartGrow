(() => {
  const root = document.getElementById('device-page');
  if (!root) return;
  let readings = [];
  const format = new Intl.NumberFormat('id-ID', {maximumFractionDigits: 1});
  const labels = {sending:'Mengirim',sent:'Menunggu perangkat',accepted:'Diterima; sedang diproses',applied:'Diterapkan perangkat',rejected:'Ditolak perangkat',rolled_back:'Kembali ke Wi-Fi lama',delivery_unknown:'Pengiriman belum terkonfirmasi',no_confirmation:'Belum ada konfirmasi akhir'};
  const units = {soil_pct:'%',air_c:'°C',humidity_pct:'%',probe_c:'°C',lux:'lux'};
  function draw() {
    const canvas = document.getElementById('chart'), ctx = canvas.getContext('2d');
    const box = canvas.getBoundingClientRect(), ratio = window.devicePixelRatio || 1;
    canvas.width = Math.max(1,box.width*ratio); canvas.height = Math.max(1,box.height*ratio);
    ctx.scale(ratio,ratio);
    const w=box.width,h=box.height,key=document.getElementById('chart-metric').value;
    const valid=readings.filter(p=>Number.isFinite(p.values[key]));
    const note=document.getElementById('chart-note');
    if (valid.length<2) {note.textContent='Menunggu setidaknya dua pembacaan valid untuk grafik.';return;}
    const min=Math.min(...valid.map(p=>p.values[key])),max=Math.max(...valid.map(p=>p.values[key]));
    const pad=Math.max((max-min)*.12,1),lo=min-pad,hi=max+pad;
    const start=new Date(readings[0].at).getTime(),end=new Date(readings.at(-1).at).getTime();
    const x=p=>48+(new Date(p.at).getTime()-start)/Math.max(1,end-start)*(w-64);
    const y=v=>18+(hi-v)/(hi-lo)*(h-52);
    ctx.font='11px system-ui';ctx.fillStyle='#66756d';ctx.strokeStyle='#e5eadf';ctx.lineWidth=1;
    for(let i=0;i<4;i++){let v=lo+(hi-lo)*i/3,py=y(v);ctx.beginPath();ctx.moveTo(48,py);ctx.lineTo(w-8,py);ctx.stroke();ctx.fillText(format.format(v),0,py+4);}
    ctx.strokeStyle='#32745a';ctx.lineWidth=2;ctx.beginPath();let previous=null;
    for(const p of readings){if(!Number.isFinite(p.values[key])){previous=null;continue;}const gap=previous && new Date(p.at)-new Date(previous.at)>45000;
      if(!previous||gap)ctx.moveTo(x(p),y(p.values[key]));else ctx.lineTo(x(p),y(p.values[key]));previous=p;}
    ctx.stroke();ctx.fillStyle='#32745a';for(const p of valid){ctx.beginPath();ctx.arc(x(p),y(p.values[key]),2,0,Math.PI*2);ctx.fill();}
    ctx.fillStyle='#66756d';const time=d=>new Date(d).toLocaleTimeString('id-ID',{hour:'2-digit',minute:'2-digit'});
    ctx.fillText(time(readings[0].at),48,h-8);ctx.fillText(time(readings.at(-1).at),Math.max(48,w-48),h-8);
    note.textContent=`${valid.length} pembacaan valid · terendah ${format.format(min)} · tertinggi ${format.format(max)}`;
  }
  async function poll() {
    try {
      const response=await fetch(root.dataset.url,{headers:{Accept:'application/json'},cache:'no-store'});
      if(!response.ok||!response.headers.get('content-type')?.includes('application/json'))throw new Error();
      const d=await response.json(),s=d.state;
      const status=document.getElementById('online-state');status.textContent=d.online?'Online':'Offline';status.classList.toggle('online',d.online);
      document.getElementById('last-seen').textContent=d.last_seen?`Data terakhir ${new Date(d.last_seen).toLocaleString('id-ID')}`:'Menunggu pembacaan pertama dari alat.';
      for(const [key,unit] of Object.entries(units))document.getElementById(key).textContent=Number.isFinite(s[key])?`${format.format(s[key])} ${unit}`:'—';
      document.getElementById('soil-note').textContent=s.soil_pct===null&&Number.isFinite(s.soil_raw)?`Belum dikalibrasi · ADC ${s.soil_raw}`:'Kelembapan relatif hasil kalibrasi';
      const note=document.getElementById('connection-note');note.hidden=d.online;note.textContent='Perangkat offline. Nilai yang ditampilkan adalah data terakhir, bukan pembacaan saat ini.';
      for(const key of ['pump','lamp']){const el=document.getElementById(`${key}-state`);el.textContent=typeof s[key]==='boolean'?(s[key]?'ON':'OFF'):'—';el.classList.toggle('online',s[key]===true);}
      document.querySelectorAll('[data-control]').forEach(b=>{b.disabled=!d.online||(b.dataset.control!=='off'&&!s[`${b.dataset.control}_enabled`]);});
      document.getElementById('control-note').textContent=!d.online?'Kontrol tersedia setelah alat online.':(!s.pump_enabled||!s.lamp_enabled)?'Sebagian kontrol belum diaktifkan. Hubungi pengelola SmartGrow.':'Perintah ON selalu memakai batas waktu otomatis.';
      const list=document.getElementById('commands');list.replaceChildren();
      for(const c of d.commands){const li=document.createElement('li'),b=document.createElement('b'),span=document.createElement('span');b.textContent=c.summary;span.textContent=labels[c.status]||c.status;li.append(b,span);list.append(li);}
      if(!d.commands.length){const li=document.createElement('li');li.textContent='Belum ada perintah.';list.append(li);}
      readings=d.readings;draw();
    } catch (_) {
      const note=document.getElementById('connection-note');note.hidden=false;note.textContent='Dashboard belum dapat memperbarui data. Periksa koneksi atau masuk kembali.';
      document.querySelectorAll('[data-control]').forEach(b=>b.disabled=true);
    }
  }
  document.getElementById('chart-metric').addEventListener('change',draw);window.addEventListener('resize',draw);
  poll();setInterval(()=>{if(!document.hidden)poll();},5000);
})();
