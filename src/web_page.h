#pragma once
// The settings page served by src/web.cpp at "/". Talks to /api/settings
// /api/status, /api/restart, /api/wifi and /api/log (JSON, format: settingsToJson in src/settings.cpp).
#include <pgmspace.h>

static const char WEB_PAGE[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="de">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Abfahrtstafel</title>
<link rel="icon" type="image/png" href="/favicon.png">
<link rel="apple-touch-icon" href="/apple-touch-icon.png">
<meta name="apple-mobile-web-app-title" content="Abfahrtstafel">
<style>
:root{--bg:#f6f4f1;--card:#fff;--fg:#222;--muted:#777;--line:#e2ddd6;--accent:#d9730d;--led:#ff7800;--ledoff:#2a2420;--panel:#120f0c}
@media (prefers-color-scheme:dark){:root{--bg:#151311;--card:#1f1c19;--fg:#eee;--muted:#9a948c;--line:#38332d;--accent:#ff8a1f;color-scheme:dark}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.4 -apple-system,system-ui,sans-serif;padding:16px 16px 90px}
main{max-width:760px;margin:0 auto}
h1{font-size:22px;margin:4px 0 2px}
h2{font-size:17px;margin:0 0 10px}
.status{color:var(--muted);font-size:13px;margin-bottom:16px}
section{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:14px;margin-bottom:14px}
.hint{color:var(--muted);font-size:13px;margin:4px 0 10px}
.row{display:flex;flex-wrap:wrap;gap:8px;align-items:center;padding:8px 0;border-top:1px solid var(--line)}
.row:first-child,h2+.row{border-top:0}
.row.paused input{opacity:.5}
label{display:inline-flex;align-items:center;gap:6px}
input,select,button{font:inherit;color:inherit}
input[type=text],input[type=time],input[type=number],select{background:var(--bg);border:1px solid var(--line);border-radius:6px;padding:5px 7px}
input[type=text]{flex:1;min-width:140px}
input[type=number]{width:80px}
input[type=range]{flex:1;min-width:120px;accent-color:var(--accent)}
input[type=checkbox]{width:18px;height:18px;accent-color:var(--accent)}
input:disabled,select:disabled{opacity:.6}
button{background:var(--bg);border:1px solid var(--line);border-radius:6px;padding:5px 10px;cursor:pointer}
button.add{margin-top:8px}
button.del{color:#c0392b}
.val{width:48px;white-space:nowrap;text-align:right;font-variant-numeric:tabular-nums}
canvas.icon{background:var(--panel);border-radius:4px;image-rendering:pixelated;vertical-align:middle}
.icons{display:flex;flex-wrap:wrap;gap:12px}
.icons figure{margin:0;text-align:center;font-size:12px;color:var(--muted)}
.editor{display:grid;grid-template-columns:repeat(12,22px);gap:2px;background:var(--panel);padding:6px;border-radius:6px;width:max-content;touch-action:none;user-select:none}
.editor div{width:22px;height:22px;border-radius:50%;background:var(--ledoff)}
.editor div.on{background:var(--led)}
.days{display:inline-flex;gap:3px}
.days button{padding:3px 0;width:30px;font-size:13px}
.days button.on{background:var(--accent);border-color:var(--accent);color:#fff}
.grp{display:inline-flex;align-items:center;gap:6px;white-space:nowrap}
.sub{display:flex;flex-wrap:wrap;gap:8px;align-items:center;width:100%;padding-left:26px}
/* narrow: times and ⏸ ✕ on top, the slider below at full width */
@media (max-width:560px){#dim .row>button:first-of-type{margin-left:auto}#dim .row>input[type=range],#dim .val{order:1}#dim .sub{order:2}}
footer{position:fixed;left:0;right:0;bottom:0;background:var(--card);border-top:1px solid var(--line);padding:10px 16px;display:flex;flex-wrap:wrap;gap:8px 12px;align-items:center;justify-content:center}
footer button{background:var(--accent);border-color:var(--accent);color:#fff;padding:8px 22px;font-weight:600}
#stale{width:100%;text-align:center;font-size:14px}
#stale button{background:var(--bg);border-color:var(--line);color:inherit;padding:5px 10px;font-weight:400;margin:2px}
#msg{font-size:14px;color:var(--muted)}
#log{background:var(--bg);border:1px solid var(--line);border-radius:6px;padding:8px;margin:0 0 8px;max-height:60vh;overflow:auto;font:12px/1.35 ui-monospace,Menlo,monospace;white-space:pre-wrap;word-break:break-all}
</style>
</head>
<body>
<main>
<h1>Abfahrtstafel</h1>
<div class="status" id="status">Lade ...</div>

<section>
<h2>Ruhezeit</h2>
<div class="hint">In dieser Zeit ist das Panel komplett aus und es werden keine Abfahrten abgerufen.</div>
<label><input type="checkbox" id="offOn"> Panel ausschalten von</label>
<span class="grp"><input type="time" id="offFrom"> bis <input type="time" id="offTo"></span>
</section>

<section>
<h2>Helligkeit</h2>
<div class="row"><button id="brightReset" title="auf den Standardwert setzen">Standard</button><input type="range" min="1" max="100" id="bright"><span class="val" id="brightVal"></span></div>
<div class="hint">Zeitfenster mit eigener Helligkeit (das erste passende gilt). ⏸ setzt ein Zeitfenster einmal aus (heute bzw. das nächste Mal) und speichert sofort.</div>
<div id="dim"></div>
<button class="add" id="addDim">+ Zeitfenster</button>
</section>

<section>
<h2>Promo-Texte</h2>
<div class="hint">Erscheinen ab und zu statt der 4. Zeile. „Rechts“ leer: Text mittig mit Icon davor. „Rechts“ gesetzt (z. B. „sofort“): wie eine Abfahrt, Icon als Linie. Ein Zeitfenster über Mitternacht zählt zu dem Wochentag, an dem es beginnt.</div>
<label>Anzeigedauer <input type="number" min="3" max="600" id="show"> s</label>
&nbsp; <label>Pause dazwischen ca. <input type="number" min="0" max="3600" id="interval"> s</label>
<div class="hint">Pause 0 = keine Promos.</div>
<label><input type="checkbox" id="fullRow"> 4. Zeile ganz für Promos (nur 3 Abfahrten)</label>
<div class="hint">Die Promos laufen dann ohne Pause der Reihe nach als Laufschrift durch (je einmal, auch die mit Text rechts; die Anzeigedauer gilt dann nicht), die Pause gilt nur noch für die Störmeldungen dazwischen. Passt gerade keine Promo (Zeitfenster, Wochentag), erscheint wieder die 4. Abfahrt.</div>
<div id="promos"></div>
<button class="add" id="addPromo">+ Eigener Text</button>
</section>

<section>
<h2>VRN-Störmeldungen</h2>
<div class="hint">Laufen abwechselnd mit den Promos durch die 4. Zeile.</div>
<label><input type="checkbox" id="disruptOn"> anzeigen</label>
&nbsp; <label><input type="number" min="1" max="10" id="passes"> Durchläufe pro Anzeige</label>
</section>
<section>
<h2>Icons</h2>
<div class="icons" id="icons"></div>
<div id="editBox" hidden>
<div class="hint">Punkte anklicken oder darüber wischen (12 × 9).</div>
<div class="editor" id="editor"></div>
<div class="row"><input type="text" id="editName" maxlength="15" placeholder="Name">
<button id="editClear">Leeren</button><button id="editDone">Fertig</button></div>
</div>
<button class="add" id="addIcon">+ Eigenes Icon</button>
</section>

<section>
<h2>WLAN</h2>
<div class="hint">Verbunden mit <b id="ssidNow">?</b>. Nach dem Speichern startet die Tafel neu. Klappt die Verbindung nicht (z. B. Tippfehler), nimmt sie nach etwa einer Minute wieder das bisherige WLAN.</div>
<div class="row"><input type="text" id="ssid" maxlength="32" placeholder="WLAN-Name" autocomplete="off" autocapitalize="off">
<input type="text" id="pass" maxlength="63" placeholder="Passwort" autocomplete="off" autocapitalize="off" spellcheck="false">
<button id="wifiSave">WLAN speichern</button></div>
<span id="wifiMsg" class="hint"></span>
</section>

<section>
<h2>Log</h2>
<div class="hint">Die letzten Meldungen der Tafel, dieselben wie am USB-Anschluss. Wird laufend ergänzt, solange es angezeigt wird.</div>
<pre id="log" hidden></pre>
<button id="logToggle">Anzeigen</button>
</section>

<section>
<h2>System</h2>
<div class="hint">Startet die Tafel neu, die Seite lädt danach von selbst neu.</div>
<button id="restart">Neu starten</button> <span id="restartMsg" class="hint"></span>
</section>
</main>
<footer><div id="stale" hidden>Die Einstellungen wurden inzwischen auf einem anderen Gerät geändert.
<button id="reload">Neu laden</button><button id="force" title="die Änderungen vom anderen Gerät gehen verloren">Trotzdem meine speichern</button></div>
<button id="save">Speichern</button><span id="msg"></span></footer>

<script>
const $=id=>document.getElementById(id);
const DAYS=['Mo','Di','Mi','Do','Fr','Sa','So'],ALL_DAYS=127;
const MAX_PROMOS=20,MAX_DIM=4,MAX_ICONS=8,IW=12,IH=9;
let S=null,editing=null;
// base: collect() as loaded, to tell unsaved changes; srv: the loaded
// settings as sent by the board, to tell real changes from a new rev alone
let base='',srv='',staleRev='';

function el(tag,props={},...kids){const e=document.createElement(tag);Object.assign(e,props);for(const k of kids)e.append(k);return e}
function time(v){return v==='24:00'?'00:00':v}
function iconRows(tag){
  const p=S.presetIcons.find(i=>i.tag===tag);
  if(p)return p.rows.map(r=>[...r].map(c=>c==='#'));
  const m=/^\[c(\d)\]$/.exec(tag),c=m&&S.icons.find(i=>i.id==m[1]);
  return c?c.rows.map(r=>[...Array(IW)].map((_,x)=>!!(r>>(IW-1-x)&1))):null;
}
function iconCanvas(tag,scale=3){
  const rows=iconRows(tag),cv=el('canvas',{className:'icon'});
  const h=rows?rows.length:IH,w=rows?rows[0].length:IW;
  cv.width=w*scale;cv.height=h*scale;
  const g=cv.getContext('2d');
  if(rows)rows.forEach((r,y)=>r.forEach((on,x)=>{g.fillStyle=on?'#ff7800':'#2a2420';g.fillRect(x*scale,y*scale,scale-(scale>2),scale-(scale>2))}));
  return cv;
}
function iconOptions(sel){
  const s=el('select');
  s.append(el('option',{value:'',textContent:'kein Icon'}));
  for(const i of S.presetIcons)s.append(el('option',{value:i.tag,textContent:i.name}));
  for(const i of S.icons)s.append(el('option',{value:'[c'+i.id+']',textContent:(i.name||'Icon '+(i.id+1))+' (eigenes)'}));
  s.value=sel;if(s.value!==sel)s.value='';
  return s;
}

function mins(v){const [h,m]=v.split(':');return h*60+ +m}
function ymd(t){const p=n=>String(n).padStart(2,'0');return t.getFullYear()+'-'+p(t.getMonth()+1)+'-'+p(t.getDate())}
// start date of the window's current occurrence, or of the next one
function nextStart(d){
  const now=new Date(),m=now.getHours()*60+now.getMinutes(),f=mins(d.from),t=mins(d.to);
  const day=n=>ymd(new Date(now.getFullYear(),now.getMonth(),now.getDate()+n));
  if(f>t&&m<t)return day(-1);
  return f<t&&m>=t?day(1):day(0);
}
function renderDim(){
  const box=$('dim');box.replaceChildren();
  S.dim.forEach((d,i)=>{
    const val=el('span',{className:'val',textContent:d.pct+' %'});
    const r=el('input',{type:'range',min:1,max:100,value:d.pct,oninput:e=>{d.pct=+e.target.value;val.textContent=d.pct+' %'}});
    const paused=!!d.skip&&d.from!==d.to&&d.skip===nextStart(d);
    box.append(el('div',{className:'row'+(paused?' paused':'')},
      el('span',{className:'grp'},el('input',{type:'time',value:time(d.from),onchange:e=>d.from=e.target.value}),'bis',
        el('input',{type:'time',value:time(d.to),onchange:e=>d.to=e.target.value})),r,val,
      el('button',{textContent:paused?'▶':'⏸',title:paused?'wieder aktivieren':'einmal aussetzen (heute bzw. das nächste Mal)',
        onclick:()=>{if(paused)delete d.skip;else d.skip=nextStart(d);save()}}),
      el('button',{className:'del',textContent:'✕',onclick:()=>{S.dim.splice(i,1);renderDim()}}),
      paused?el('div',{className:'sub',style:'padding-left:0;color:var(--muted);font-size:13px',textContent:'Einmal ausgesetzt, danach gilt das Zeitfenster wieder.'}):''));
  });
  $('addDim').disabled=S.dim.length>=MAX_DIM;
}

function renderPromos(){
  const box=$('promos');box.replaceChildren();
  S.promos.forEach((p,i)=>{
    const pre=!!p.preset,dis={};
    const prev=el('span');const upd=()=>prev.replaceChildren(p.icon?iconCanvas(p.icon,2):'');upd();
    const sel=iconOptions(p.icon||'');Object.assign(sel,dis,{onchange:e=>{p.icon=e.target.value;upd()}});
    const win=p.win||{from:'',to:''};
    const from=el('input',{type:'time',value:win.from?time(win.from):'',...dis,onchange:e=>{win.from=e.target.value;p.win=win}});
    const to=el('input',{type:'time',value:win.to?time(win.to):'',...dis,onchange:e=>{win.to=e.target.value;p.win=win}});
    // weekday toggles; the last one left can't be switched off
    const days=el('span',{className:'days'});
    DAYS.forEach((n,d)=>{
      const b=el('button',{textContent:n,title:'an diesem Wochentag anzeigen'});
      const show=()=>b.classList.toggle('on',!!((p.days??ALL_DAYS)&1<<d));show();
      b.onclick=()=>{const v=(p.days??ALL_DAYS)^1<<d;if(v){p.days=v;show()}};
      days.append(b);
    });
    box.append(el('div',{className:'row'},
      el('input',{type:'checkbox',checked:p.on,title:'anzeigen',onchange:e=>p.on=e.target.checked}),
      prev,
      el('input',{type:'text',value:p.text,maxLength:60,placeholder:'Text',...dis,oninput:e=>p.text=e.target.value}),
      pre?el('button',{textContent:'↺',title:'Vorgabe wiederherstellen',onclick:()=>{Object.assign(p,{icon:'',text:'',right:'',win:null,days:ALL_DAYS},JSON.parse(JSON.stringify(p.def)));renderPromos()}}):el('button',{className:'del',textContent:'✕',onclick:()=>{S.promos.splice(i,1);renderPromos()}}),
      el('div',{className:'sub'},el('span',{className:'grp'},'Icon',sel),el('span',{className:'grp'},'Rechts',
        el('input',{type:'text',value:p.right,maxLength:15,placeholder:'leer',style:'width:90px',...dis,oninput:e=>p.right=e.target.value})),
        el('span',{className:'grp'},'nur von',from,'bis',to),days)));
  });
  $('addPromo').disabled=S.promos.length>=MAX_PROMOS;
}

function renderIcons(){
  const box=$('icons');box.replaceChildren();
  for(const i of S.presetIcons)box.append(el('figure',{},iconCanvas(i.tag),el('figcaption',{textContent:i.name})));
  for(const i of S.icons){
    box.append(el('figure',{},iconCanvas('[c'+i.id+']'),el('figcaption',{textContent:i.name||'Icon '+(i.id+1)}),
      el('div',{},el('button',{textContent:'Ändern',onclick:()=>edit(i)}),' ',
        el('button',{className:'del',textContent:'✕',onclick:()=>delIcon(i)}))));
  }
  $('addIcon').disabled=S.icons.length>=MAX_ICONS;
}

function delIcon(i){
  const tag='[c'+i.id+']';
  if(S.promos.some(p=>p.icon===tag)&&!confirm('Das Icon wird noch benutzt. Trotzdem löschen?'))return;
  S.icons=S.icons.filter(x=>x!==i);
  for(const p of S.promos)if(p.icon===tag)p.icon='';
  if(editing===i)$('editBox').hidden=true;
  renderIcons();renderPromos();
}

function edit(i){
  editing=i;$('editBox').hidden=false;$('editName').value=i.name;
  const ed=$('editor');ed.replaceChildren();
  let paint=null;
  for(let y=0;y<IH;y++)for(let x=0;x<IW;x++){
    const bit=1<<(IW-1-x),d=el('div');
    const set=on=>{i.rows[y]=on?i.rows[y]|bit:i.rows[y]&~bit;d.classList.toggle('on',on)};
    set(!!(i.rows[y]&bit));
    d.onpointerdown=e=>{e.preventDefault();if(d.hasPointerCapture(e.pointerId))d.releasePointerCapture(e.pointerId);paint=!(i.rows[y]&bit);set(paint)};
    d.onpointerenter=()=>{if(paint!==null)set(paint)};
    d.dataset.x=x;d.dataset.y=y;d._set=set;
    ed.append(d);
  }
  // touch: pointerenter doesn't fire while the finger moves
  ed.onpointermove=e=>{if(paint===null)return;const t=document.elementFromPoint(e.clientX,e.clientY);if(t&&t._set&&t.parentNode===ed)t._set(paint)};
  window.onpointerup=()=>{if(paint!==null){paint=null;renderIcons();renderPromos()}};
  $('editBox').scrollIntoView({behavior:'smooth',block:'center'});
}
$('editName').oninput=e=>{if(editing){editing.name=e.target.value;renderIcons();renderPromos()}};
$('editClear').onclick=()=>{if(editing){editing.rows.fill(0);edit(editing);renderIcons();renderPromos()}};
$('editDone').onclick=()=>{$('editBox').hidden=true;editing=null;renderIcons();renderPromos()};
$('addIcon').onclick=()=>{
  let id=0;while(S.icons.some(i=>i.id===id))id++;
  const i={id,name:'',rows:Array(IH).fill(0)};S.icons.push(i);renderIcons();edit(i);
};
$('addPromo').onclick=()=>{S.promos.push({on:true,icon:'',text:'',right:''});renderPromos();
  const ins=$('promos').querySelectorAll('input[type=text]');ins[ins.length-2].focus()};
$('addDim').onclick=()=>{S.dim.push({from:'22:00',to:'06:00',pct:20});renderDim()};
function setBright(v){S.brightnessPct=v;$('bright').value=v;$('brightVal').textContent=v+' %'}
$('bright').oninput=e=>setBright(+e.target.value);
$('disruptOn').onchange=e=>$('passes').disabled=!e.target.checked;
$('brightReset').onclick=()=>setBright(S.brightnessDefaultPct);

function render(){
  $('offOn').checked=S.off.on;$('offFrom').value=time(S.off.from);$('offTo').value=time(S.off.to);
  setBright(S.brightnessPct);
  $('show').value=S.promoShowS;$('interval').value=S.promoIntervalS;$('fullRow').checked=S.promoFullRow;
  $('disruptOn').checked=S.disruptOn;$('passes').value=S.disruptPasses;$('passes').disabled=!S.disruptOn;
  renderDim();renderPromos();renderIcons();
}

function collect(){
  const promos=S.promos.map(p=>{
    const o={on:p.on,icon:p.icon||'',text:p.text,right:p.right};
    if(p.preset)o.preset=p.preset;
    if(p.win&&p.win.from&&p.win.to&&p.win.from!==p.win.to)o.win={from:p.win.from,to:p.win.to};
    if((p.days??ALL_DAYS)!==ALL_DAYS)o.days=p.days;
    return o;
  }).filter(p=>p.preset||p.text||p.icon);
  return {off:{on:$('offOn').checked,from:$('offFrom').value||'02:00',to:$('offTo').value||'06:00'},
    brightnessPct:S.brightnessPct,dim:S.dim.filter(d=>d.from&&d.to),
    promoShowS:+$('show').value,promoIntervalS:+$('interval').value,promoFullRow:$('fullRow').checked,
    disruptOn:$('disruptOn').checked,disruptPasses:+$('passes').value,promos,icons:S.icons};
}

const dirty=()=>JSON.stringify(collect())!==base;
const content=s=>JSON.stringify({...s,rev:''});
function apply(s){
  S=s;srv=content(s);staleRev='';
  if(editing){$('editBox').hidden=true;editing=null}
  render();base=JSON.stringify(collect());$('stale').hidden=true;
}
async function getSettings(){
  const r=await fetch('/api/settings',{cache:'no-store'});
  if(!r.ok)throw new Error(await r.text());
  return r.json();
}
async function load(){apply(await getSettings())}
// The board reports a different rev (saved on another device, or restarted):
// take over its settings, but ask first if that would discard changes here.
let syncing=false;
async function sync(rev){
  if(!S||!rev||rev===S.rev||rev===staleRev||syncing)return;
  syncing=true;
  try{
    const s=await getSettings();
    // still the same settings (e.g. only restarted): keep any changes here
    if(content(s)===srv)S.rev=s.rev;
    // a time field being typed in only reports its value when done
    else if(!dirty()&&!document.activeElement?.matches('input[type=time]'))apply(s);
    else{staleRev=s.rev;$('stale').hidden=false}
  }catch(e){}
  syncing=false;
}
let msgTimer=0;
// force: without ?rev=, so it also overwrites changes from another device
async function save(force){
  clearTimeout(msgTimer);
  $('msg').textContent='Speichere ...';
  try{
    const r=await fetch('/api/settings'+(force?'':'?rev='+encodeURIComponent(S.rev)),{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(collect())});
    if(r.status===409){$('msg').textContent='Nicht gespeichert';$('stale').hidden=false;renderDim();return}
    if(!r.ok)throw new Error(await r.text());
    apply(await r.json());
    $('msg').textContent='Gespeichert ✓';
    msgTimer=setTimeout(()=>$('msg').textContent='',3000);
  }catch(e){$('msg').textContent='Fehler: '+e.message;renderDim()}
}
$('save').onclick=()=>save();
$('force').onclick=()=>save(true);
$('reload').onclick=()=>{
  if(dirty()&&!confirm('Deine nicht gespeicherten Änderungen gehen verloren.'))return;
  $('msg').textContent='';
  load().catch(e=>$('msg').textContent='Laden fehlgeschlagen: '+e.message);
};

async function status(){
  try{
    const s=await (await fetch('/api/status',{cache:'no-store'})).json();
    const age=s.dataAgeS<0?'noch keine Daten':'Daten von vor '+(s.dataAgeS<120?s.dataAgeS+' s':Math.round(s.dataAgeS/60)+' min');
    const up=s.uptimeS<7200?Math.round(s.uptimeS/60)+' min':Math.round(s.uptimeS/3600)+' h';
    $('ssidNow').textContent=s.ssid;
    $('status').textContent=`Firmware ${s.build} · läuft seit ${up} · ${age} · ${s.off?'Ruhezeit':'Panel an'} · WLAN ${s.rssi} dBm`;
    sync(s.rev);
  }catch(e){$('status').textContent='Tafel nicht erreichbar'}
}
// waits for the board to come back after a restart, then reloads the page
async function afterRestart(msg){
  const t0=Date.now();
  await new Promise(r=>setTimeout(r,5000));
  for(;;){
    try{const s=await (await fetch('/api/status',{cache:'no-store'})).json();if(s.uptimeS<120){location.reload();return}}catch(e){}
    if(Date.now()-t0>120000){msg.textContent='Tafel meldet sich nicht, bitte Seite neu laden.';return}
    await new Promise(r=>setTimeout(r,2000));
  }
}
$('restart').onclick=async()=>{
  if(!confirm('Tafel jetzt neu starten? Nicht gespeicherte Änderungen gehen verloren.'))return;
  $('restart').disabled=true;$('restartMsg').textContent='Starte neu ...';
  try{const r=await fetch('/api/restart',{method:'POST'});if(!r.ok)throw new Error(await r.text())}
  catch(e){$('restartMsg').textContent='Fehler: '+e.message;$('restart').disabled=false;return}
  afterRestart($('restartMsg'));
};
$('wifiSave').onclick=async()=>{
  const ssid=$('ssid').value.trim(),pass=$('pass').value;
  if(!ssid){$('wifiMsg').textContent='Bitte WLAN-Namen eingeben.';return}
  if(!confirm('Tafel mit „'+ssid+'“ verbinden und neu starten? Nicht gespeicherte Änderungen gehen verloren.'))return;
  $('wifiSave').disabled=true;$('wifiMsg').textContent='Speichere und starte neu ...';
  try{const r=await fetch('/api/wifi',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid,pass})});if(!r.ok)throw new Error(await r.text())}
  catch(e){$('wifiMsg').textContent='Fehler: '+e.message;$('wifiSave').disabled=false;return}
  $('wifiMsg').textContent='Gespeichert, Tafel verbindet sich neu. Ist das neue WLAN ein anderes, ist die Seite erst wieder erreichbar, wenn dieses Gerät auch darin ist.';
  afterRestart($('wifiMsg'));
};
// polls /api/log while shown and the tab is visible, appending what is new
// since the last poll (rarely: each request holds up the network task)
const LOG_KEEP=200000,LOG_POLL_MS=10000;
let logBoot=null,logNext=0,logTimer=0,logOn=false,logGen=0;
async function pollLog(){
  const gen=logGen;
  if(document.hidden){logTimer=setTimeout(pollLog,LOG_POLL_MS);return}
  try{
    const r=await fetch('/api/log?since='+logNext,{cache:'no-store'});
    if(!r.ok)throw new Error(r.status);
    const boot=r.headers.get('X-Log-Boot'),start=+r.headers.get('X-Log-Start'),t=await r.text();
    if(gen!==logGen)return;  // hidden (and maybe shown again) meanwhile
    const p=$('log'),atEnd=p.scrollTop+p.clientHeight>=p.scrollHeight-20;
    let add='';
    if(boot!==logBoot){if(logBoot!==null)add='––– Neustart –––\n';logBoot=boot;logNext=0}
    if(start!==logNext)add+='…\n';
    add+=t;logNext=+r.headers.get('X-Log-Next');
    if(add){let s=p.textContent+add;if(s.length>LOG_KEEP)s=s.slice(s.indexOf('\n',s.length-LOG_KEEP)+1);p.textContent=s}
    if(atEnd)p.scrollTop=p.scrollHeight;
  }catch(e){}
  if(logOn&&gen===logGen)logTimer=setTimeout(pollLog,LOG_POLL_MS);
}
// back in the foreground: catch up at once
document.addEventListener('visibilitychange',()=>{if(logOn&&!document.hidden){clearTimeout(logTimer);logGen++;pollLog()}});
$('logToggle').onclick=()=>{
  logOn=!logOn;$('log').hidden=!logOn;$('logToggle').textContent=logOn?'Ausblenden':'Anzeigen';
  clearTimeout(logTimer);logGen++;if(logOn)pollLog();
};
load().catch(e=>$('msg').textContent='Laden fehlgeschlagen: '+e.message);
// back in the foreground (also from the back/forward cache): update at once
document.addEventListener('visibilitychange',()=>{if(!document.hidden)status()});
window.addEventListener('pageshow',e=>{if(e.persisted)status()});
status();setInterval(()=>{if(!document.hidden)status()},15000);
</script>
</body>
</html>
)rawliteral";
