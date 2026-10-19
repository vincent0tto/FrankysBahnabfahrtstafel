#pragma once
// The event page served by src/web.cpp at "/event": switches the event mode
// and edits its scenes. Talks to /api/event, /api/event/on,
// /api/event/preview and /api/status (JSON, format: eventToJson in src/event.cpp).
#include <pgmspace.h>

static const char WEB_EVENT_PAGE[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="de">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Event – Abfahrtstafel</title>
<link rel="icon" type="image/png" href="/favicon.png">
<link rel="apple-touch-icon" href="/apple-touch-icon.png">
<style>
:root{--bg:#f6f4f1;--card:#fff;--fg:#222;--muted:#6b6560;--line:#e2ddd6;--accent:#b4560a;--panel:#0b0908}
@media (prefers-color-scheme:dark){:root{--bg:#151311;--card:#1f1c19;--fg:#eee;--muted:#a39d95;--line:#38332d;--accent:#e07a1f;color-scheme:dark}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.4 -apple-system,system-ui,sans-serif;padding:16px 16px 90px}
main{max-width:760px;margin:0 auto}
a{color:var(--accent)}
h1{font-size:22px;margin:4px 0 2px}
h2{font-size:17px;margin:0 0 10px}
h3{font-size:14px;margin:14px 0 6px}
.back{display:inline-block;font-size:14px;text-decoration:none;padding:4px 0}
.status{color:var(--muted);font-size:13px;margin-bottom:16px}
section{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:14px;margin-bottom:14px}
section.on{border:2px solid var(--accent)}
.hint{color:var(--muted);font-size:13px;margin:4px 0 10px}
.note{background:var(--bg);border-radius:6px;padding:6px 10px;font-size:13px;margin-top:10px}
input,select,button{font:inherit;color:inherit}
input[type=text],input[type=number]{background:var(--bg);border:1px solid var(--line);border-radius:6px;padding:6px 8px}
input[type=text]{flex:1;min-width:140px}
input[type=number]{width:80px}
input[type=range]{flex:1;min-width:120px;accent-color:var(--accent)}
input[type=color]{width:34px;height:34px;padding:1px;border:1px solid var(--line);border-radius:50%;background:none;cursor:pointer}
button{background:var(--bg);border:1px solid var(--line);border-radius:6px;padding:5px 10px;cursor:pointer}
button:disabled{opacity:.5;cursor:default}
button.add{margin-top:8px}
button.del{color:#c0392b}
.row{display:flex;flex-wrap:wrap;gap:8px;align-items:center}
.val{width:48px;text-align:right;font-variant-numeric:tabular-nums}
.head{display:flex;align-items:center;gap:12px}
.head>div{flex:1}
.switch{width:60px;height:34px;border-radius:17px;border:0;padding:3px;background:#cfc9c1;flex:none;transition:background .2s}
.switch span{display:block;width:28px;height:28px;border-radius:50%;background:#fff;box-shadow:0 1px 3px rgba(0,0,0,.3);transition:transform .2s}
.switch[aria-checked=true]{background:var(--accent)}
.switch[aria-checked=true] span{transform:translateX(26px)}
canvas.board{display:block;width:100%;aspect-ratio:3/1;background:var(--panel);border-radius:6px;image-rendering:pixelated}
.cap{color:var(--muted);font-size:13px;margin-top:6px}
.scene{border-top:1px solid var(--line);padding:8px 0}
.scene:first-of-type{border-top:0}
.shead{display:flex;gap:8px;align-items:center}
.chip{flex:1;min-width:0;cursor:pointer;display:flex;gap:10px;align-items:center}
.chip .led{width:96px;height:32px;flex:none;border-radius:4px;background:var(--panel);display:flex;align-items:center;justify-content:center;overflow:hidden;font:600 12px ui-monospace,Menlo,monospace;white-space:nowrap;padding:0 4px}
.chip b{display:block}
.chip small{color:var(--muted);font-size:13px}
.scene.open .chip .led{outline:2px solid var(--accent);outline-offset:1px}
.sbody{padding:10px 0 4px}
.field{display:flex;flex-wrap:wrap;gap:6px 10px;align-items:center;padding:5px 0}
.flabel{width:96px;color:var(--muted);font-size:13px}
.fctl{display:flex;flex-wrap:wrap;gap:6px 8px;align-items:center;flex:1;min-width:200px}
.line{background:var(--bg);border-radius:8px;padding:8px 10px;margin-top:8px}
.line .field{padding:4px 0}
.swatches{display:flex;flex-wrap:wrap;gap:8px;align-items:center}
.sw{width:34px;height:34px;border-radius:50%;padding:0;border:1px solid rgba(0,0,0,.2)}
.sw.on{box-shadow:0 0 0 2px var(--card),0 0 0 4px var(--accent)}
.seg{display:flex;border:1px solid var(--line);border-radius:8px;overflow:hidden;width:100%;max-width:420px}
.seg button{flex:1;border:0;border-radius:0;border-left:1px solid var(--line);padding:7px 4px;font-size:14px}
.seg button:first-child{border-left:0}
.seg button.on{background:var(--accent);color:#fff;font-weight:600}
@media (max-width:560px){.flabel{width:100%}}
footer{position:fixed;left:0;right:0;bottom:0;background:var(--card);border-top:1px solid var(--line);padding:10px 16px;display:flex;flex-wrap:wrap;gap:8px 12px;align-items:center;justify-content:center}
footer button{background:var(--accent);border-color:var(--accent);color:#fff;padding:8px 22px;font-weight:600}
footer button:disabled{background:var(--bg);border-color:var(--line);color:var(--muted);font-weight:400;opacity:1}
#stale{width:100%;text-align:center;font-size:14px}
#stale button{background:var(--bg);border-color:var(--line);color:inherit;padding:5px 10px;font-weight:400;margin:2px}
#msg{font-size:14px;color:var(--muted)}
</style>
</head>
<body>
<main>
<a class="back" href="/">‹ Abfahrtstafel</a>
<h1>Event</h1>
<div class="status" id="status">Lade ...</div>

<section id="modeCard">
<div class="head"><div><h2 style="margin:0">Event-Modus</h2><div class="hint" id="modeText" style="margin:2px 0 0"></div></div>
<button class="switch" id="modeSwitch" role="switch" aria-checked="false" aria-label="Event-Modus an oder aus"><span></span></button></div>
<div class="note" id="modeNote" hidden>Die Ruhezeit gilt im Event-Modus nicht: Die Tafel bleibt an, bis du ihn wieder ausschaltest.</div>
</section>

<section>
<h2>Vorschau</h2>
<canvas class="board" id="board" width="768" height="256"></canvas>
<div class="cap" id="boardCap"></div>
</section>

<section>
<h2>Szenen</h2>
<div class="hint">Laufen der Reihe nach in einer Schleife. Auf eine Szene tippen, um sie zu bearbeiten; die Vorschau oben zeigt sie dann.</div>
<div id="scenes"></div>
<button class="add" id="addScene">+ Szene</button> <span class="hint" id="sceneCount"></span>
</section>

<section>
<h2>Helligkeit im Event</h2>
<div class="row"><input type="range" min="1" max="100" id="bright" aria-label="Helligkeit im Event"><span class="val" id="brightVal"></span><button id="brightReset" title="auf den Standardwert setzen">↺</button></div>
<div class="hint">Gilt, solange der Event-Modus an ist. Die Zeitfenster aus dem Zeitplan zählen dann nicht.</div>
</section>
</main>
<footer><div id="stale" hidden>Das Event wurde inzwischen auf einem anderen Gerät geändert.
<button id="reload">Neu laden</button><button id="force" title="die Änderungen vom anderen Gerät gehen verloren">Trotzdem meine speichern</button></div>
<button id="save" disabled>Alles gespeichert</button><span id="msg"></span></footer>

<script>
const $=id=>document.getElementById(id);
const MAX_SCENES=8,MAX_LINES=4,W=192,H=64,DOT=4;
const COLORS=[['#ff7800','Amber'],['#ffd400','Gelb'],['#ff3030','Rot'],['#ff3bd0','Pink'],['#8a5bff','Lila'],
  ['#2f7bff','Blau'],['#29c6ff','Cyan'],['#4be04b','Grün'],['#ffffff','Weiß']];
const BGS=[['#000000','Schwarz'],['#1a0b2e','Dunkellila'],['#2a0a0a','Dunkelrot'],['#06213a','Nachtblau'],['#0a2a12','Dunkelgrün'],['#2a1a05','Dunkelbraun']];
const SIZES=[['small','Klein'],['normal','Normal'],['large','Groß'],['huge','Riesig']];
const VPOS=[['top','Oben'],['middle','Mitte'],['bottom','Unten']];
const HALIGN=[['left','Links'],['center','Mitte'],['right','Rechts']];
let S=null,open=0,leaving=false,base='',srv='',staleRev='';

function el(tag,props={},...kids){const e=document.createElement(tag);Object.assign(e,props);for(const k of kids)e.append(k);return e}
function seg(opts,cur,pick){
  return el('div',{className:'seg'},...opts.map(([v,t])=>el('button',{type:'button',textContent:t,className:v===cur?'on':'',onclick:()=>pick(v)})));
}
function field(label,...kids){return el('div',{className:'field'},el('span',{className:'flabel',textContent:label}),el('div',{className:'fctl'},...kids))}
// preset colours plus a picker for any other one
function swatches(list,cur,pick){
  const box=el('div',{className:'swatches'});
  for(const [c,name] of list)box.append(el('button',{type:'button',className:'sw'+(c===cur?' on':''),title:name,ariaLabel:name,style:'background:'+c,onclick:()=>pick(c)}));
  const own=!list.some(([c])=>c===cur);
  box.append(el('input',{type:'color',value:cur,title:'eigene Farbe',ariaLabel:'eigene Farbe',className:own?'sw on':'',oninput:e=>pick(e.target.value,true)}));
  return box;
}

// ---- preview: the board draws the scene itself (/api/event/preview) ----
let pvTimer=0,pvBusy=false,pvAgain=false;
function preview(){clearTimeout(pvTimer);pvTimer=setTimeout(drawPreview,250)}
async function drawPreview(){
  if(pvBusy){pvAgain=true;return}
  const sc=S&&S.scenes[open];
  $('boardCap').textContent=sc?`Szene ${open+1}: ${sc.name||'ohne Namen'} · so zeigt sie die Tafel`:'Keine Szene';
  const g=$('board').getContext('2d');
  let px=new Uint16Array(W*H);
  if(sc){
    pvBusy=true;
    try{
      const r=await fetch('/api/event/preview?ms=0',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(sc)});
      if(r.ok)px=new Uint16Array(await r.arrayBuffer());
    }catch(e){}
    pvBusy=false;
  }
  g.fillStyle='#000';g.fillRect(0,0,W*DOT,H*DOT);
  for(let y=0;y<H;y++)for(let x=0;x<W;x++){
    const c=px[y*W+x];
    g.fillStyle=c?`rgb(${(c>>11)*255/31|0},${(c>>5&63)*255/63|0},${(c&31)*255/31|0})`:'#1c1814';
    g.beginPath();g.arc(x*DOT+DOT/2,y*DOT+DOT/2,DOT*.42,0,7);g.fill();
  }
  if(pvAgain){pvAgain=false;drawPreview()}
}

// ---- scenes ----
function newLine(){return {text:'Neue Zeile',color:'#ff7800',size:'normal',v:'middle',h:'center'}}
function lineBody(sc,l,i){
  const upd=()=>{renderScenes(false);preview()};
  return el('div',{className:'line'},
    field('Zeile '+(i+1),el('input',{type:'text',value:l.text,maxLength:60,placeholder:'Text',oninput:e=>{l.text=e.target.value;upd()}}),
      el('button',{className:'del',textContent:'✕',title:'Zeile löschen',ariaLabel:'Zeile löschen',onclick:()=>{sc.lines.splice(i,1);renderScenes();preview()}})),
    field('Farbe',swatches(COLORS,l.color,(c,typing)=>{l.color=c;typing?upd():(renderScenes(),preview())})),
    field('Schrift',seg(SIZES,l.size,v=>{l.size=v;renderScenes();preview()})),
    field('Höhe',seg(VPOS,l.v,v=>{l.v=v;renderScenes();preview()})),
    field('Ausrichtung',seg(HALIGN,l.h,v=>{l.h=v;renderScenes();preview()})));
}
function sceneBody(sc,i){
  const b=el('div',{className:'sbody'},
    field('Name',el('input',{type:'text',value:sc.name,maxLength:22,placeholder:'z. B. Happy Hour',oninput:e=>{sc.name=e.target.value;renderScenes(false);preview()}})),
    field('Anzeigedauer',el('input',{type:'number',min:2,max:600,value:sc.durS,oninput:e=>{sc.durS=+e.target.value;renderScenes(false)}}),'Sekunden'),
    field('Hintergrund',swatches(BGS,sc.bg,(c,typing)=>{sc.bg=c;typing?preview():(renderScenes(),preview())})),
    el('div',{className:'hint',textContent:'Dunkle Töne wirken am besten. Ein heller Hintergrund lässt alle LEDs leuchten und macht die Tafel sehr hell.'}));
  sc.lines.forEach((l,j)=>b.append(lineBody(sc,l,j)));
  b.append(el('button',{className:'add',textContent:'+ Zeile',disabled:sc.lines.length>=MAX_LINES,
    onclick:()=>{sc.lines.push(newLine());renderScenes();preview()}}));
  const move=d=>{const j=i+d;[S.scenes[i],S.scenes[j]]=[S.scenes[j],S.scenes[i]];open=j;renderScenes()};
  b.append(el('div',{className:'field'},
    el('button',{textContent:'▲ nach oben',disabled:i===0,onclick:()=>move(-1)}),
    el('button',{textContent:'▼ nach unten',disabled:i===S.scenes.length-1,onclick:()=>move(1)}),
    el('button',{className:'del',textContent:'Szene löschen',onclick:()=>{
      if(!confirm('Szene „'+(sc.name||i+1)+'“ löschen?'))return;
      S.scenes.splice(i,1);open=Math.min(open,S.scenes.length-1);renderScenes();preview()}})));
  return b;
}
// full: also the open scene's editor (not while typing in it, keeps the focus)
function renderScenes(full=true){
  const box=$('scenes');
  if(!full){
    box.querySelectorAll('.scene').forEach((card,i)=>card.querySelector('.chip').replaceWith(chip(S.scenes[i],i)));
    return;
  }
  box.replaceChildren();
  S.scenes.forEach((sc,i)=>{
    const card=el('div',{className:'scene'+(i===open?' open':'')});
    card.append(el('div',{className:'shead'},chip(sc,i),
      el('button',{textContent:i===open?'▴':'▾',ariaLabel:i===open?'zuklappen':'bearbeiten',onclick:()=>toggle(i)})));
    if(i===open)card.append(sceneBody(sc,i));
    box.append(card);
  });
  $('addScene').disabled=S.scenes.length>=MAX_SCENES;
  $('sceneCount').textContent=S.scenes.length+' von '+MAX_SCENES;
  updateSave();
}
function toggle(i){open=open===i?-1:i;renderScenes();if(open>=0)preview()}
function chip(sc,i){
  const l=sc.lines.find(l=>l.text)||{text:'(leer)',color:'#777'};
  const total=S.scenes.reduce((a,s)=>a+(+s.durS||0),0);
  return el('div',{className:'chip',onclick:()=>toggle(i)},
    el('span',{className:'led',style:`color:${l.color};background:${sc.bg==='#000000'?'':sc.bg}`,textContent:l.text}),
    el('span',{},el('b',{textContent:(i+1)+'. '+(sc.name||'Szene')}),
      el('small',{textContent:`${sc.durS} s · ${sc.lines.length} ${sc.lines.length===1?'Zeile':'Zeilen'}`+(i===0&&S.scenes.length>1?` · Durchlauf ${total} s`:'')})));
}
$('addScene').onclick=()=>{
  S.scenes.push({name:'',durS:8,bg:'#000000',lines:[newLine()]});
  open=S.scenes.length-1;renderScenes();preview();
};

function setBright(v){S.brightnessPct=v;$('bright').value=v;$('brightVal').textContent=v+' %'}
$('bright').oninput=e=>setBright(+e.target.value);
$('brightReset').onclick=()=>setBright(S.brightnessDefaultPct);

// ---- on / off ----
function renderMode(){
  const on=!!S.on;
  $('modeSwitch').setAttribute('aria-checked',on);
  $('modeCard').classList.toggle('on',on);
  $('modeText').textContent=on?'An – die Tafel zeigt das Event':'Aus – die Tafel zeigt die Abfahrten';
  $('modeNote').hidden=!on;
}
$('modeSwitch').onclick=async()=>{
  const want=!S.on;
  if(want&&dirty()&&!confirm('Es gibt ungespeicherte Änderungen. Die Tafel zeigt zunächst den gespeicherten Stand. Trotzdem einschalten?'))return;
  $('modeSwitch').disabled=true;
  try{
    const r=await fetch('/api/event/on',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({on:want})});
    if(!r.ok)throw new Error(await r.text());
    const s=await r.json();
    S.on=s.on;S.rev=s.rev;srv=content(s);renderMode();
  }catch(e){$('msg').textContent='Fehler: '+e.message}
  $('modeSwitch').disabled=false;
};

// ---- load, save, keep in sync (as on the main page) ----
function collect(){
  return {brightnessPct:S.brightnessPct,scenes:S.scenes.map(sc=>({name:sc.name,durS:Math.min(600,Math.max(2,+sc.durS||8)),bg:sc.bg,
    lines:sc.lines.map(l=>({text:l.text,color:l.color,size:l.size,v:l.v,h:l.h}))}))};
}
const dirty=()=>!!S&&JSON.stringify(collect())!==base;
function updateSave(){
  if(!S)return;
  const d=dirty();
  $('save').disabled=!d;$('save').textContent=d?'Änderungen speichern':'Alles gespeichert';
}
for(const ev of ['input','change','click'])document.addEventListener(ev,updateSave);
window.addEventListener('beforeunload',e=>{if(!leaving&&dirty())e.preventDefault()});
const content=s=>JSON.stringify({...s,rev:'',on:false});
function apply(s){
  srv=content(s);S=s;staleRev='';
  if(open>=S.scenes.length)open=S.scenes.length-1;
  setBright(S.brightnessPct);renderMode();renderScenes();
  base=JSON.stringify(collect());$('stale').hidden=true;updateSave();preview();
}
async function getEvent(){
  const r=await fetch('/api/event',{cache:'no-store'});
  if(!r.ok)throw new Error(await r.text());
  return r.json();
}
async function load(){apply(await getEvent())}
let syncing=false;
async function sync(rev){
  if(!S||!rev||rev===S.rev||rev===staleRev||syncing)return;
  syncing=true;
  try{
    const s=await getEvent();
    if(content(s)===srv){S.rev=s.rev;S.on=s.on;renderMode()}  // only switched on/off or restarted
    else if(!dirty())apply(s);
    else{staleRev=s.rev;S.on=s.on;renderMode();$('stale').hidden=false}
  }catch(e){}
  syncing=false;
}
let msgTimer=0;
async function save(force){
  clearTimeout(msgTimer);
  $('msg').textContent='Speichere ...';
  try{
    const r=await fetch('/api/event'+(force?'':'?rev='+encodeURIComponent(S.rev)),{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(collect())});
    if(r.status===409){$('msg').textContent='Nicht gespeichert';$('stale').hidden=false;return}
    if(!r.ok)throw new Error(await r.text());
    apply(await r.json());
    $('msg').textContent='Gespeichert ✓';
    msgTimer=setTimeout(()=>$('msg').textContent='',3000);
  }catch(e){$('msg').textContent='Fehler: '+e.message}
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
    const up=s.uptimeS<7200?Math.round(s.uptimeS/60)+' min':Math.round(s.uptimeS/3600)+' h';
    $('status').textContent=`Firmware ${s.build} · läuft seit ${up} · ${s.event?'Event läuft':'Abfahrten'} · WLAN ${s.rssi} dBm`;
    sync(s.eventRev);
  }catch(e){$('status').textContent='Tafel nicht erreichbar'}
}
load().catch(e=>$('msg').textContent='Laden fehlgeschlagen: '+e.message);
document.addEventListener('visibilitychange',()=>{if(!document.hidden)status()});
window.addEventListener('pageshow',e=>{if(e.persisted)status()});
status();setInterval(()=>{if(!document.hidden)status()},15000);
</script>
</body>
</html>
)rawliteral";
