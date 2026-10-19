#pragma once
// The event page served by src/web.cpp at "/event": switches the event mode
// and edits its scenes and pixel images. Talks to /api/event, /api/event/on,
// /api/event/preview, /api/event/test(/stop), /api/images, /api/image and
// /api/status (JSON, format: eventToJson in src/event.cpp; images:
// src/web.cpp).
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
.sw.rainbow{background:conic-gradient(#ff4040,#ffb000,#ffe600,#4be04b,#29c6ff,#8a5bff,#ff3bd0,#ff4040)}
.chips{display:flex;flex-wrap:wrap;gap:6px}
.chips button{border-radius:16px;padding:5px 12px}
.chips button.on{background:var(--accent);border-color:var(--accent);color:#fff;font-weight:600}
.pvbar{display:flex;flex-wrap:wrap;gap:8px;align-items:center;margin-top:8px}
.pvbar .cap{flex:1;margin:0;min-width:180px}
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
.thumbs{display:grid;grid-template-columns:repeat(auto-fill,minmax(150px,1fr));gap:12px}
.thumb{background:none;border:0;padding:0;text-align:left;cursor:pointer;font-size:14px}
.thumb canvas,.thumb .new{display:block;width:100%;aspect-ratio:3/1;border-radius:6px}
.thumb canvas{background:var(--panel);image-rendering:pixelated}
.thumb.on canvas{outline:2px solid var(--accent);outline-offset:2px}
.thumb .new{border:1.5px dashed var(--line);display:flex;align-items:center;justify-content:center;font-size:24px;color:var(--muted)}
.thumb small{display:block;color:var(--muted);font-size:12px}
#editor{border-top:1px solid var(--line);margin-top:14px;padding-top:4px}
.zoomwrap{overflow:auto;background:var(--panel);border-radius:6px;margin:8px 0;max-height:70vh}
#edCanvas{display:block;width:100%;image-rendering:pixelated;cursor:crosshair}
.frames{display:flex;flex-wrap:wrap;gap:8px}
.frames canvas{width:96px;aspect-ratio:3/1;background:var(--panel);border-radius:4px;cursor:pointer;image-rendering:pixelated}
.frames canvas.on{outline:2px solid var(--accent);outline-offset:1px}
.pal{display:flex;flex-wrap:wrap;gap:6px;align-items:center}
.pal .sw{width:30px;height:30px;border-radius:6px}
button.primary{background:var(--accent);border-color:var(--accent);color:#fff;font-weight:600}
.filebtn{display:inline-flex;align-items:center;background:var(--bg);border:1px solid var(--line);border-radius:6px;padding:5px 10px;cursor:pointer}
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
<div class="pvbar"><div class="cap" id="boardCap"></div>
<button id="play">▶ Abspielen</button><button id="test" title="zeigt die Szene 30 Sekunden auf der Tafel, ohne zu speichern">Auf der Tafel testen</button></div>
<div class="hint" id="testMsg" style="margin:6px 0 0"></div>
</section>

<section>
<h2>Szenen</h2>
<div class="hint">Laufen der Reihe nach in einer Schleife. Auf eine Szene tippen, um sie zu bearbeiten; die Vorschau oben zeigt sie dann.</div>
<div id="scenes"></div>
<button class="add" id="addScene">+ Szene</button> <span class="hint" id="sceneCount"></span>
</section>

<section>
<h2>Bilder</h2>
<div class="hint">Pixelgrafiken über die ganze Tafel (192 × 64) für den Hintergrund einer Szene, unter ihren Texten. Selbst zeichnen oder ein Bild laden; mehrere Einzelbilder laufen als Animation.</div>
<div class="thumbs" id="thumbs"></div>
<div class="hint" id="space" style="margin-top:10px"></div>
<div id="editor" hidden>
<h3 id="edTitle"></h3>
<div class="field"><span class="flabel">Name</span><div class="fctl"><input type="text" id="edName" maxlength="22" placeholder="z. B. Cocktail"></div></div>
<div id="edTools"></div>
<div class="zoomwrap" id="edWrap"><canvas id="edCanvas" width="1152" height="384"></canvas></div>
<div class="row"><div id="edZoom" style="flex:1;min-width:220px"></div><button id="edUndo">↶ Rückgängig</button><button id="edClear">Leeren</button></div>
<div class="field"><span class="flabel">Farbe</span><div class="fctl"><div class="pal" id="edPal"></div></div></div>
<h3>Einzelbilder</h3>
<div class="frames" id="edFrames"></div>
<div class="row" style="margin-top:8px"><button id="edAddFrame">+ Kopie</button><button id="edDelFrame" class="del">Einzelbild entfernen</button><button id="edPlay">▶ Abspielen</button></div>
<div class="row" style="margin-top:8px"><span class="flabel">Bildwechsel</span><input type="range" id="edDelay" min="50" max="2000" step="50" aria-label="Bildwechsel"><span class="val" id="edDelayVal" style="width:70px"></span></div>
<h3>Aus einer Datei</h3>
<div class="hint">PNG, JPG oder GIF, wird auf 192 × 64 verkleinert und ersetzt das Bild oben. Aus einem animierten GIF werden (in Chrome) bis zu 8 Einzelbilder.</div>
<div class="row"><div id="edFit" style="flex:1;min-width:200px"></div><label class="filebtn">Datei wählen …<input type="file" id="edFile" accept="image/*" hidden></label></div>
<div class="row" style="margin-top:14px"><button id="edSave" class="primary">Bild speichern</button><button id="edClose">Schließen</button><button id="edDelete" class="del">Bild löschen</button><span class="hint" id="edMsg" style="margin:0"></span></div>
</div>
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
const ANIMS=[['none','Keine'],['scroll','Laufschrift'],['blink','Blinken'],['pulse','Pulsieren'],['type','Schreibmaschine'],['wave','Welle']];
const TRANS=[['cut','Schnitt'],['slide','Schieben'],['wipe','Wischen']];
const VPOS=[['top','Oben'],['middle','Mitte'],['bottom','Unten']];
const HALIGN=[['left','Links'],['center','Mitte'],['right','Rechts']];
let S=null,open=0,leaving=false,base='',srv='',staleRev='';

function el(tag,props={},...kids){const e=document.createElement(tag);Object.assign(e,props);for(const k of kids)e.append(k);return e}
function seg(opts,cur,pick){
  return el('div',{className:'seg'},...opts.map(([v,t])=>el('button',{type:'button',textContent:t,className:v===cur?'on':'',onclick:()=>pick(v)})));
}
function field(label,...kids){return el('div',{className:'field'},el('span',{className:'flabel',textContent:label}),el('div',{className:'fctl'},...kids))}
// preset colours (and the rainbow) plus a picker for any other one
function swatches(list,cur,pick,rainbow){
  const box=el('div',{className:'swatches'});
  for(const [c,name] of list)box.append(el('button',{type:'button',className:'sw'+(c===cur?' on':''),title:name,ariaLabel:name,style:'background:'+c,onclick:()=>pick(c)}));
  if(rainbow)box.append(el('button',{type:'button',className:'sw rainbow'+(cur==='rainbow'?' on':''),title:'Regenbogen',ariaLabel:'Regenbogen',onclick:()=>pick('rainbow')}));
  const own=cur!=='rainbow'&&!list.some(([c])=>c===cur);
  box.append(el('input',{type:'color',value:cur==='rainbow'?'#ff7800':cur,title:'eigene Farbe',ariaLabel:'eigene Farbe',className:own?'sw on':'',oninput:e=>pick(e.target.value,true)}));
  return box;
}
function chips(opts,cur,pick){
  return el('div',{className:'chips'},...opts.map(([v,t])=>el('button',{type:'button',textContent:t,className:v===cur?'on':'',onclick:()=>pick(v)})));
}

// ---- preview: the board draws the scene itself (/api/event/preview) ----
// After a change it plays for PLAY_MS (frames as fast as the board answers,
// at most every FRAME_MS), then stops to spare the board; ▶ plays again.
const PLAY_MS=20000,FRAME_MS=150;
let pvTimer=0,pvBusy=false,pvAgain=false,playFrom=0,playing=false,nextFrame=0;
function preview(){clearTimeout(pvTimer);pvTimer=setTimeout(()=>{startPlay();drawPreview()},250)}
function startPlay(){playFrom=performance.now();setPlaying(true)}
function setPlaying(on){playing=on;$('play').textContent=on?'⏸ Anhalten':'▶ Abspielen'}
$('play').onclick=()=>{if(playing)setPlaying(false);else{startPlay();drawPreview()}};
async function drawPreview(){
  clearTimeout(nextFrame);  // one chain of frames only
  if(pvBusy){pvAgain=true;return}
  const sc=S&&S.scenes[open];
  $('boardCap').textContent=sc?`Szene ${open+1}: ${sc.name||'ohne Namen'} · so zeigt sie die Tafel`:'Keine Szene';
  $('test').disabled=!sc&&!testEnd;
  const t0=performance.now(),ms=Math.round(t0-playFrom);
  if(ms>=PLAY_MS||document.hidden)setPlaying(false);
  let px=new Uint16Array(W*H);
  if(sc){
    pvBusy=true;
    try{
      const r=await fetch('/api/event/preview?ms='+(playing?ms:0),{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(sc)});
      if(r.ok){
        const runs=new Uint16Array(await r.arrayBuffer());
        for(let i=0,p=0;i+1<runs.length&&p<px.length;i+=2){px.fill(runs[i+1],p,p+runs[i]);p+=runs[i]}
      }
    }catch(e){}
    pvBusy=false;
  }
  const g=$('board').getContext('2d');
  g.fillStyle='#000';g.fillRect(0,0,W*DOT,H*DOT);
  for(let y=0;y<H;y++)for(let x=0;x<W;x++){
    const c=px[y*W+x];
    g.fillStyle=c?`rgb(${(c>>11)*255/31|0},${(c>>5&63)*255/63|0},${(c&31)*255/31|0})`:'#1c1814';
    g.beginPath();g.arc(x*DOT+DOT/2,y*DOT+DOT/2,DOT*.42,0,7);g.fill();
  }
  if(pvAgain){pvAgain=false;drawPreview()}
  else if(playing&&sc)nextFrame=setTimeout(drawPreview,Math.max(0,FRAME_MS-(performance.now()-t0)));
}
// While a test runs (TEST_S on the board) the button ends it instead.
const TEST_S=30;
let testEnd=0,testTimer=0;
function showTest(){
  const left=Math.ceil((testEnd-Date.now())/1000);
  if(left<=0){clearInterval(testTimer);testEnd=0;$('test').textContent='Auf der Tafel testen';$('testMsg').textContent='';return}
  $('test').textContent=`■ Test beenden (${left} s)`;
}
$('test').onclick=async()=>{
  const sc=S&&S.scenes[open];
  try{
    if(testEnd){
      const r=await fetch('/api/event/test/stop',{method:'POST'});
      if(!r.ok)throw new Error(await r.text());
      testEnd=Date.now();showTest();
      return;
    }
    if(!sc)return;
    const r=await fetch('/api/event/test',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(sc)});
    if(!r.ok)throw new Error(await r.text());
    $('testMsg').textContent='Die Tafel zeigt die Szene, danach wieder wie vorher.';
    testEnd=Date.now()+TEST_S*1000;clearInterval(testTimer);testTimer=setInterval(showTest,1000);showTest();
  }catch(e){$('testMsg').textContent='Fehler: '+e.message}
};

// ---- scenes ----
function newLine(){return {text:'Neue Zeile',color:'#ff7800',size:'normal',v:'middle',h:'center',anim:'none',speed:5}}
function lineBody(sc,l,i){
  const upd=()=>{renderScenes(false);preview()};
  return el('div',{className:'line'},
    field('Zeile '+(i+1),el('input',{type:'text',value:l.text,maxLength:60,placeholder:'Text',oninput:e=>{l.text=e.target.value;upd()}}),
      el('button',{className:'del',textContent:'✕',title:'Zeile löschen',ariaLabel:'Zeile löschen',onclick:()=>{sc.lines.splice(i,1);renderScenes();preview()}})),
    field('Farbe',swatches(COLORS,l.color,(c,typing)=>{l.color=c;typing?upd():(renderScenes(),preview())},true)),
    field('Schrift',seg(SIZES,l.size,v=>{l.size=v;renderScenes();preview()})),
    field('Höhe',seg(VPOS,l.v,v=>{l.v=v;renderScenes();preview()})),
    field('Ausrichtung',seg(HALIGN,l.h,v=>{l.h=v;renderScenes();preview()})),
    field('Animation',chips(ANIMS,l.anim,v=>{l.anim=v;renderScenes();preview()})),
    l.anim!=='none'||l.color==='rainbow'?field('Tempo',el('span',{className:'hint',style:'margin:0',textContent:'langsam'}),
      el('input',{type:'range',min:1,max:10,value:l.speed,ariaLabel:'Tempo',oninput:e=>{l.speed=+e.target.value;preview()}}),
      el('span',{className:'hint',style:'margin:0',textContent:'schnell'})):'');
}
function sceneBody(sc,i){
  const b=el('div',{className:'sbody'},
    field('Name',el('input',{type:'text',value:sc.name,maxLength:22,placeholder:'z. B. Happy Hour',oninput:e=>{sc.name=e.target.value;renderScenes(false);preview()}})),
    field('Anzeigedauer',el('input',{type:'number',min:2,max:600,value:sc.durS,oninput:e=>{sc.durS=+e.target.value;renderScenes(false)}}),'Sekunden'),
    field('Hintergrund',swatches(BGS,sc.bg,(c,typing)=>{sc.bg=c;typing?preview():(renderScenes(),preview())})),
    el('div',{className:'hint',textContent:'Dunkle Töne wirken am besten. Ein heller Hintergrund lässt alle LEDs leuchten und macht die Tafel sehr hell.'}),
    field('Bild',imageSelect(sc)),
    field('Übergang',seg(TRANS,sc.trans,v=>{sc.trans=v;renderScenes()}),
      el('span',{className:'hint',style:'margin:0',textContent:'zur nächsten Szene'})));
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
function imageSelect(sc){
  const sel=el('select',{onchange:e=>{sc.img=+e.target.value;renderScenes();preview()}});
  sel.append(el('option',{value:-1,textContent:'kein Bild'}));
  for(const im of IM.images)sel.append(el('option',{value:im.id,textContent:im.name||'Bild '+(im.id+1)}));
  const cur=sc.img??-1;
  if(cur>=0&&!IM.images.some(im=>im.id===cur))sel.append(el('option',{value:cur,textContent:'(gelöscht)'}));
  sel.value=cur;
  return el('span',{},sel,' ',el('a',{href:'#editor',textContent:'Bilder bearbeiten',onclick:e=>{e.preventDefault();
    if(cur>=0&&IM.images.some(im=>im.id===cur))openEditor(cur);else $('thumbs').scrollIntoView({behavior:'smooth'})}}));
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
  const color=l.color==='rainbow'?'#fff':l.color;
  const total=S.scenes.reduce((a,s)=>a+(+s.durS||0),0);
  return el('div',{className:'chip',onclick:()=>toggle(i)},
    el('span',{className:'led',style:`color:${color};background:${sc.bg==='#000000'?'':sc.bg}`,textContent:l.text}),
    el('span',{},el('b',{textContent:(i+1)+'. '+(sc.name||'Szene')}),
      el('small',{textContent:`${sc.durS} s · ${sc.lines.length} ${sc.lines.length===1?'Zeile':'Zeilen'}`+
        (S.scenes.length>1&&sc.trans!=='cut'?' · '+TRANS.find(t=>t[0]===sc.trans)[1]:'')+(i===0&&S.scenes.length>1?` · Durchlauf ${total} s`:'')})));
}
$('addScene').onclick=()=>{
  S.scenes.push({name:'',durS:8,bg:'#000000',trans:'cut',lines:[newLine()]});
  open=S.scenes.length-1;renderScenes();preview();
};

function setBright(v){S.brightnessPct=v;$('bright').value=v;$('brightVal').textContent=v+' %'}
$('bright').oninput=e=>setBright(+e.target.value);
$('brightReset').onclick=()=>setBright(S.brightnessDefaultPct);

// ---- pixel images: list and editor ----
const PAL=['#ff7800','#ffb000','#ffd400','#ff3030','#ff2a4a','#ff3bd0','#ff5fae','#8a5bff',
  '#2f7bff','#29c6ff','#cfefff','#4be04b','#7ee04a','#ffe9a8','#ffffff','#6b4a2a'];
const TOOLS=[['pen','Stift'],['erase','Radierer'],['fill','Füllen'],['pick','Pipette'],['move','Verschieben']];
const ZOOMS=[[1,'Ganz'],[2,'2×'],[4,'4×'],[8,'8×']];
const FITS=[['contain','Einpassen'],['cover','Ausfüllen']];
const CELL=6;  // editor canvas pixels per LED
let IM={images:[],freeBytes:0,totalBytes:0,frameBytes:W*H*2,maxFrames:8,maxImages:16},thumbs={},ED=null;
const to565=hex=>{const v=parseInt(hex.slice(1),16),r=v>>16,g=v>>8&255,b=v&255;return (r&0xF8)<<8|(g&0xFC)<<3|b>>3};
const rgbOf=c=>[(c>>11)*255/31|0,(c>>5&63)*255/63|0,(c&31)*255/31|0];
const hexOf=c=>'#'+rgbOf(c).map(v=>v.toString(16).padStart(2,'0')).join('');
// a frame into a 192 x 64 canvas, 1:1
function drawSmall(cv,px){
  cv.width=W;cv.height=H;
  const g=cv.getContext('2d'),d=g.createImageData(W,H);
  for(let i=0;i<W*H;i++){const [r,gg,b]=px[i]?rgbOf(px[i]):[11,9,8];d.data.set([r,gg,b,255],i*4)}
  g.putImageData(d,0,0);
}
async function loadImages(){
  IM=await (await fetch('/api/images',{cache:'no-store'})).json();
  renderThumbs();if(S)renderScenes();
  for(const im of IM.images){  // one after the other, the board serves one request at a time
    if(thumbs[im.id])continue;
    try{const r=await fetch(`/api/image?id=${im.id}&frame=0`,{cache:'no-store'});if(r.ok)thumbs[im.id]=new Uint16Array(await r.arrayBuffer())}catch(e){}
    renderThumbs();
  }
}
function renderThumbs(){
  const box=$('thumbs');box.replaceChildren();
  for(const im of IM.images){
    const cv=el('canvas');drawSmall(cv,thumbs[im.id]||new Uint16Array(W*H));
    box.append(el('button',{type:'button',className:'thumb'+(ED&&ED.id===im.id?' on':''),onclick:()=>openEditor(im.id)},cv,
      el('b',{textContent:im.name||'Bild '+(im.id+1)}),el('small',{textContent:im.frames>1?im.frames+' Einzelbilder, animiert':'1 Einzelbild'})));
  }
  if(IM.images.length<IM.maxImages)box.append(el('button',{type:'button',className:'thumb',onclick:()=>openEditor(null)},
    el('span',{className:'new',textContent:'+'}),el('b',{textContent:'Neues Bild'})));
  const left=Math.max(0,Math.floor((IM.freeBytes-16384)/(IM.frameBytes+512)));
  $('space').textContent=`${IM.images.length} von ${IM.maxImages} Bildern · ${Math.round((IM.totalBytes-IM.freeBytes)/1024)} von ${Math.round(IM.totalBytes/1024)} KB belegt · Platz für etwa ${left} weitere Einzelbilder`;
}
async function openEditor(id){
  if(ED&&ED.dirty&&!confirm('Das Bild hat ungespeicherte Änderungen. Verwerfen?'))return;
  stopPlay();
  const info=IM.images.find(i=>i.id===id);
  let frames=[new Uint16Array(W*H)];
  if(info){
    try{
      const r=await fetch('/api/image?id='+id,{cache:'no-store'});
      if(!r.ok)throw new Error(await r.text());
      const all=new Uint16Array(await r.arrayBuffer());
      frames=[];for(let i=0;i<info.frames;i++)frames.push(all.slice(i*W*H,(i+1)*W*H));
    }catch(e){alert('Bild laden fehlgeschlagen: '+e.message);return}
  }
  ED={id:info?id:-1,name:info?info.name:'',delay:info?info.delayMs:300,frames,cur:0,tool:'pen',color:'#ff7800',zoom:ED?ED.zoom:1,fit:'contain',undo:[],dirty:false,play:0};
  $('editor').hidden=false;$('edMsg').textContent='';
  renderEditor();renderThumbs();
  $('editor').scrollIntoView({behavior:'smooth',block:'start'});
}
function renderEditor(){
  $('edTitle').textContent=ED.id<0?'Neues Bild':'Bild bearbeiten';
  $('edName').value=ED.name;
  $('edTools').replaceChildren(seg(TOOLS,ED.tool,v=>{ED.tool=v;renderEditor()}));
  $('edZoom').replaceChildren(seg(ZOOMS,ED.zoom,v=>{ED.zoom=v;renderEditor()}));
  $('edFit').replaceChildren(seg(FITS,ED.fit,v=>{ED.fit=v;renderEditor()}));
  const cv=$('edCanvas');
  cv.style.width=ED.zoom*100+'%';
  cv.style.touchAction=ED.tool==='move'?'auto':'none';
  cv.style.cursor=ED.tool==='move'?'grab':'crosshair';
  const pal=$('edPal');pal.replaceChildren();
  for(const c of PAL)pal.append(el('button',{type:'button',className:'sw'+(c===ED.color?' on':''),style:'background:'+c,ariaLabel:c,
    onclick:()=>{ED.color=c;if(ED.tool==='erase'||ED.tool==='pick')ED.tool='pen';renderEditor()}}));
  pal.append(el('input',{type:'color',value:ED.color,title:'eigene Farbe',ariaLabel:'eigene Farbe',className:PAL.includes(ED.color)?'':'sw on',
    onchange:e=>{ED.color=e.target.value;if(ED.tool==='erase'||ED.tool==='pick')ED.tool='pen';renderEditor()}}));
  renderFrames();
  $('edDelay').value=ED.delay;$('edDelayVal').textContent=ED.delay+' ms';
  $('edDelay').disabled=$('edPlay').disabled=ED.frames.length<2;
  $('edAddFrame').disabled=ED.frames.length>=IM.maxFrames;
  $('edDelFrame').disabled=ED.frames.length<2;
  $('edUndo').disabled=!ED.undo.length;
  $('edDelete').hidden=ED.id<0;
  $('edSave').disabled=!ED.dirty;
  drawEditor();
}
function renderFrames(){
  const box=$('edFrames');box.replaceChildren();
  ED.frames.forEach((f,i)=>{
    const cv=el('canvas',{className:i===ED.cur?'on':'',title:'Einzelbild '+(i+1),onclick:()=>{stopPlay();ED.cur=i;renderEditor()}});
    drawSmall(cv,f);box.append(cv);
  });
}
function drawEditor(px=ED.frames[ED.cur]){
  const g=$('edCanvas').getContext('2d');
  g.fillStyle='#000';g.fillRect(0,0,W*CELL,H*CELL);
  for(let i=0;i<W*H;i++)paintCell(g,i%W,i/W|0,px[i]);
}
function paintCell(g,x,y,c){g.fillStyle=c?hexOf(c):'#241f1b';g.fillRect(x*CELL,y*CELL,CELL-1,CELL-1)}
function markDirty(){ED.dirty=true;$('edSave').disabled=false;$('edUndo').disabled=!ED.undo.length}
function pushUndo(){ED.undo.push({cur:ED.cur,frames:ED.frames.map(f=>f.slice())});if(ED.undo.length>20)ED.undo.shift()}
$('edUndo').onclick=()=>{const u=ED.undo.pop();if(!u)return;stopPlay();ED.frames=u.frames;ED.cur=Math.min(u.cur,u.frames.length-1);markDirty();renderEditor()};
$('edClear').onclick=()=>{stopPlay();pushUndo();ED.frames[ED.cur].fill(0);markDirty();renderEditor()};
$('edName').oninput=e=>{ED.name=e.target.value;markDirty()};
// drawing: pen and eraser along the pointer (lines between the events), fill, colour picker
let drawing=false,last=null;
function cellAt(e){
  const r=$('edCanvas').getBoundingClientRect();
  return {x:Math.min(W-1,Math.max(0,Math.floor((e.clientX-r.left)/r.width*W))),y:Math.min(H-1,Math.max(0,Math.floor((e.clientY-r.top)/r.height*H)))};
}
function plot(p){
  const v=ED.tool==='erase'?0:to565(ED.color);
  ED.frames[ED.cur][p.y*W+p.x]=v;paintCell($('edCanvas').getContext('2d'),p.x,p.y,v);
}
function fill(p){
  const f=ED.frames[ED.cur],from=f[p.y*W+p.x],to=to565(ED.color);
  if(from===to)return;
  const stack=[p.y*W+p.x];
  while(stack.length){
    const i=stack.pop();if(f[i]!==from)continue;f[i]=to;
    const x=i%W;
    if(x>0)stack.push(i-1);if(x<W-1)stack.push(i+1);if(i>=W)stack.push(i-W);if(i<W*(H-1))stack.push(i+W);
  }
}
const cv=$('edCanvas');
cv.onpointerdown=e=>{
  if(!ED||ED.tool==='move')return;
  e.preventDefault();stopPlay();try{cv.setPointerCapture(e.pointerId)}catch(err){}
  const p=cellAt(e),c=ED.frames[ED.cur][p.y*W+p.x];
  if(ED.tool==='pick'){if(c){ED.color=hexOf(c);ED.tool='pen';renderEditor()}return}
  pushUndo();
  if(ED.tool==='fill'){fill(p);markDirty();renderEditor();return}
  drawing=true;last=p;plot(p);
};
cv.onpointermove=e=>{
  if(!drawing)return;
  const p=cellAt(e),n=Math.max(Math.abs(p.x-last.x),Math.abs(p.y-last.y));
  for(let k=1;k<=n;k++)plot({x:Math.round(last.x+(p.x-last.x)*k/n),y:Math.round(last.y+(p.y-last.y)*k/n)});
  last=p;
};
cv.onpointerup=cv.onpointercancel=()=>{if(drawing){drawing=false;markDirty();renderFrames();$('edUndo').disabled=false}};
// frames
$('edAddFrame').onclick=()=>{stopPlay();pushUndo();ED.frames.splice(ED.cur+1,0,ED.frames[ED.cur].slice());ED.cur++;markDirty();renderEditor()};
$('edDelFrame').onclick=()=>{stopPlay();pushUndo();ED.frames.splice(ED.cur,1);ED.cur=Math.min(ED.cur,ED.frames.length-1);markDirty();renderEditor()};
$('edDelay').oninput=e=>{ED.delay=+e.target.value;$('edDelayVal').textContent=ED.delay+' ms';markDirty();if(ED.play){stopPlay();startEdPlay()}};
function startEdPlay(){
  let k=0;
  ED.play=setInterval(()=>{k=(k+1)%ED.frames.length;drawEditor(ED.frames[k])},ED.delay);
  $('edPlay').textContent='⏸ Anhalten';
}
function stopPlay(){if(ED&&ED.play){clearInterval(ED.play);ED.play=0;$('edPlay').textContent='▶ Abspielen';drawEditor()}}
$('edPlay').onclick=()=>{if(ED.play)stopPlay();else startEdPlay()};
// import: scaled to the board (pixel art by whole steps, without blurring), transparent stays dark
function toPixels(src){
  const w=src.displayWidth||src.width,h=src.displayHeight||src.height;
  let s=ED.fit==='cover'?Math.max(W/w,H/h):Math.min(W/w,H/h);
  const c=el('canvas',{width:W,height:H}),g=c.getContext('2d');
  if(s>=2){s=Math.floor(s);g.imageSmoothingEnabled=false}else g.imageSmoothingQuality='high';
  g.drawImage(src,Math.round((W-w*s)/2),Math.round((H-h*s)/2),w*s,h*s);
  const d=g.getImageData(0,0,W,H).data,px=new Uint16Array(W*H);
  for(let i=0;i<W*H;i++)if(d[i*4+3]>=128)px[i]=(d[i*4]&0xF8)<<8|(d[i*4+1]&0xFC)<<3|d[i*4+2]>>3;
  return px;
}
$('edFile').onchange=async e=>{
  const file=e.target.files[0];e.target.value='';
  if(!file)return;
  if(ED.frames.some(f=>f.some(v=>v))&&!confirm('Das ersetzt das Bild oben. Weiter?'))return;
  stopPlay();
  let frames=[],delay=null;
  try{
    if(window.ImageDecoder&&file.type!=='image/jpeg'){
      const dec=new ImageDecoder({data:await file.arrayBuffer(),type:file.type});
      await dec.tracks.ready;
      const n=Math.min(dec.tracks.selectedTrack.frameCount,IM.maxFrames);
      for(let i=0;i<n;i++){
        const {image}=await dec.decode({frameIndex:i});
        frames.push(toPixels(image));
        if(i===0&&image.duration)delay=Math.round(image.duration/1000);
        image.close();
      }
    }
  }catch(err){frames=[]}
  try{
    if(!frames.length){const bmp=await createImageBitmap(file);frames.push(toPixels(bmp));bmp.close()}
  }catch(err){alert('Das Bild lässt sich nicht lesen.');return}
  pushUndo();
  ED.frames=frames;ED.cur=0;
  if(delay)ED.delay=Math.min(2000,Math.max(50,Math.round(delay/50)*50));
  if(!ED.name)ED.name=file.name.replace(/\.[^.]*$/,'').slice(0,22);
  markDirty();renderEditor();
};
$('edSave').onclick=async()=>{
  stopPlay();
  $('edMsg').textContent='Speichere ...';$('edSave').disabled=true;
  const fd=new FormData();
  fd.append('px',new Blob(ED.frames),'px.bin');
  try{
    const r=await fetch(`/api/image?id=${ED.id}&frames=${ED.frames.length}&delay=${ED.delay}&name=${encodeURIComponent(ED.name)}`,{method:'POST',body:fd});
    if(!r.ok)throw new Error(await r.text());
    IM=await r.json();
    ED.id=IM.id;ED.dirty=false;thumbs[ED.id]=ED.frames[0].slice();
    $('edMsg').textContent='Gespeichert ✓';
    renderEditor();renderThumbs();if(S)renderScenes();preview();
  }catch(e){$('edMsg').textContent='Fehler: '+e.message;$('edSave').disabled=false}
};
$('edClose').onclick=()=>{
  if(ED.dirty&&!confirm('Das Bild hat ungespeicherte Änderungen. Verwerfen?'))return;
  stopPlay();ED=null;$('editor').hidden=true;renderThumbs();
};
// The board also takes the image out of the scenes that show it (and saves
// the event). Unsaved changes here stay, with that applied to them too.
$('edDelete').onclick=async()=>{
  const used=S.scenes.filter(sc=>sc.img===ED.id).length;
  if(!confirm('Bild „'+(ED.name||'ohne Namen')+'“ löschen?'+(used?` ${used===1?'Eine Szene zeigt':used+' Szenen zeigen'} es, dort fällt es weg.`:'')))return;
  stopPlay();
  try{
    const id=ED.id,r=await fetch('/api/image/delete?id='+id,{method:'POST'});
    if(!r.ok)throw new Error(await r.text());
    IM=await r.json();delete thumbs[id];
    const s=await getEvent();
    if(!dirty())apply(s);
    else{S.rev=s.rev;srv=content(s);S.scenes.forEach(sc=>{if(sc.img===id)sc.img=-1})}
    ED=null;$('editor').hidden=true;renderThumbs();renderScenes();preview();
  }catch(e){$('edMsg').textContent='Fehler: '+e.message}
};

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
  return {brightnessPct:S.brightnessPct,scenes:S.scenes.map(sc=>({name:sc.name,durS:Math.min(600,Math.max(2,+sc.durS||8)),bg:sc.bg,img:sc.img??-1,trans:sc.trans,
    lines:sc.lines.map(l=>({text:l.text,color:l.color,size:l.size,v:l.v,h:l.h,anim:l.anim,speed:l.speed}))}))};
}
const dirty=()=>!!S&&JSON.stringify(collect())!==base;
function updateSave(){
  if(!S)return;
  const d=dirty();
  $('save').disabled=!d;$('save').textContent=d?'Änderungen speichern':'Alles gespeichert';
}
for(const ev of ['input','change','click'])document.addEventListener(ev,updateSave);
window.addEventListener('beforeunload',e=>{if(!leaving&&(dirty()||ED&&ED.dirty))e.preventDefault()});
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
load().then(loadImages).catch(e=>$('msg').textContent='Laden fehlgeschlagen: '+e.message);
document.addEventListener('visibilitychange',()=>{if(!document.hidden)status()});
document.addEventListener('visibilitychange',()=>{if(document.hidden)setPlaying(false)});
window.addEventListener('pageshow',e=>{if(e.persisted)status()});
status();setInterval(()=>{if(!document.hidden)status()},15000);
</script>
</body>
</html>
)rawliteral";
