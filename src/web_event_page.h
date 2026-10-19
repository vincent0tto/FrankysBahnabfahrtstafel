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
#edCanvas{display:block;width:100%;image-rendering:pixelated;cursor:crosshair;touch-action:none}
.frames{display:flex;flex-wrap:wrap;gap:8px}
.frames canvas{width:96px;aspect-ratio:3/1;background:var(--panel);border-radius:4px;cursor:pointer;image-rendering:pixelated}
.frames canvas.on{outline:2px solid var(--accent);outline-offset:1px}
.pal{display:flex;flex-wrap:wrap;gap:6px;align-items:center}
.pal .sw{width:30px;height:30px;border-radius:6px}
button.primary{background:var(--accent);border-color:var(--accent);color:#fff;font-weight:600}
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
<div class="hint">Pixelgrafiken über die ganze Tafel (192 × 64) für den Hintergrund einer Szene, unter ihren Texten. Selbst zeichnen oder mit einer Vorlage (z. B. dem FRANKY'S-Logo) anfangen; mehrere Einzelbilder laufen als Animation.</div>
<div class="thumbs" id="thumbs"></div>
<div class="hint" id="space" style="margin-top:10px"></div>
<div id="editor" hidden>
<h3 id="edTitle"></h3>
<div class="field"><span class="flabel">Name</span><div class="fctl"><input type="text" id="edName" maxlength="22" placeholder="z. B. Cocktail"></div></div>
<div id="edTools"></div>
<div class="row" id="edOpts" style="margin-top:8px"></div>
<div class="zoomwrap" id="edWrap"><canvas id="edCanvas" width="1152" height="384"></canvas></div>
<div class="hint" id="edHint" style="margin-top:0"></div>
<div class="row"><div id="edZoom" style="flex:1;min-width:220px"></div><button id="edUndo" title="Strg+Z">↶ Rückgängig</button><button id="edRedo" title="Strg+Umschalt+Z">↷ Wiederholen</button><button id="edClear">Leeren</button></div>
<div class="field"><span class="flabel">Farbe</span><div class="fctl"><div class="pal" id="edPal"></div></div></div>
<div class="field"><span class="flabel">Verschieben</span><div class="fctl"><button id="edLeft" aria-label="nach links">←</button><button id="edUp" aria-label="nach oben">↑</button><button id="edDown" aria-label="nach unten">↓</button><button id="edRight" aria-label="nach rechts">→</button><button id="edFlip" title="das Einzelbild waagerecht spiegeln">⇋ Umdrehen</button></div></div>
<div class="field"><span class="flabel">Hilfen</span><div class="fctl"><label><input type="checkbox" id="edMirror"> Spiegeln</label><label><input type="checkbox" id="edOnion"> vorheriges Einzelbild zeigen</label></div></div>
<h3>Einzelbilder</h3>
<div class="frames" id="edFrames"></div>
<div class="row" style="margin-top:8px"><button id="edAddFrame">+ Kopie</button><button id="edDelFrame" class="del">Einzelbild entfernen</button><button id="edPlay">▶ Abspielen</button></div>
<div class="row" style="margin-top:8px"><span class="flabel">Bildwechsel</span><input type="range" id="edDelay" min="50" max="2000" step="50" aria-label="Bildwechsel"><span class="val" id="edDelayVal" style="width:70px"></span></div>
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
// the panel's darkest visible level, each channel 64 of 255 (darker stays black)
const BGS=[['#000000','Schwarz'],['#400040','Dunkellila'],['#400000','Dunkelrot'],['#000040','Nachtblau'],['#004000','Dunkelgrün'],['#004040','Dunkeltürkis']];
// How the panel shows a colour: the library maps each channel (from RGB565)
// through a CIE lightness table onto 16 levels (cie_luts.h, PIXEL_COLOR_DEPTH_BITS=4);
// LEVEL_AT holds the first input value of each level, below 55 it stays off.
// ledRGB gives that on screen (null = off), so all previews here match the board.
const LEVEL_AT=[0,55,97,122,142,158,171,184,194,204,214,222,230,238,245,252];
const lvl=v=>{let l=0;while(l<15&&v>=LEVEL_AT[l+1])l++;return l};
const SHOW=LEVEL_AT.map((_,l)=>Math.round(255*Math.pow(l/15,1/2.2)));
function ledRGB(c){
  if(!c)return null;
  let r=c>>8&0xF8,g=c>>3&0xFC,b=c<<3&0xF8;r|=r>>5;g|=g>>6;b|=b>>5;
  const L=[lvl(r),lvl(g),lvl(b)];
  return L[0]||L[1]||L[2]?L.map(l=>SHOW[l]):null;
}
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
    const led=ledRGB(c);
    g.fillStyle=led?`rgb(${led})`:'#1c1814';
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
    el('div',{className:'hint',textContent:'Dunkle Töne wirken am besten, ein heller Hintergrund macht die Tafel sehr hell. Noch dunklere Farben als diese zeigt die Tafel nicht, sie bleiben schwarz.'}),
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
const TOOLS=[['pen','Stift'],['line','Linie'],['rect','Rechteck'],['circle','Kreis'],['fill','Füllen'],
  ['text','Text'],['icon','Symbol'],['tpl','Vorlage'],['erase','Radierer'],['pick','Pipette']];
const TOOL_HINTS={pen:'Malen mit dem Finger oder der Maus.',
  line:'Ziehen von Anfang bis Ende. Rastet waagerecht, senkrecht und diagonal ein.',
  rect:'Ziehen von einer Ecke zur anderen.',circle:'Ziehen von der Mitte nach außen.',
  fill:'Tippen füllt die zusammenhängende Fläche gleicher Farbe.',
  text:'Tippen oder ziehen setzt den Text, so wie die Tafel ihn schreibt.',
  icon:'Tippen oder ziehen setzt das Symbol.',tpl:'Tippen oder ziehen setzt die Vorlage in der gewählten Farbe, „In die Mitte“ setzt sie mittig.',
  erase:'Wischen macht Pixel aus (schwarz = durchsichtig).',
  pick:'Tippen übernimmt die Farbe eines Pixels.'};
const ZOOMS=[[1,'Ganz'],[2,'2×'],[4,'4×'],[8,'8×']];
// Vorlagen: 1-bit pictures, one hex string per row (made from the FRANKY'S logos)
const TEMPLATES=[
  {key:'logo',name:"FRANKY'S Logo",w:64,rows:[
   '0000000000000000','000007fbf0000000','bfff7e000fe7f3f8','c000000000007f86','c000000000000004','4000000000000004',
   '4000000000000004','4000c70000000002','40ffdfc0300c0002','43ff9fe0300c0082','c30018e0300e0182','c3001830780e0182',
   'c3001830780f0182','c3f01810680d0182','83f018106c0d8382','c38018306c0d8381','c3001830c40cc301','83001870c60cc301',
   '83001fe0c60c4301','83001fc0ce0c6301','83001e00ff0c2301','c3001f00c10c3301','c3001b01818c1301','c3001981818c1b01',
   '430019c1808c0f01','c30018c180cc0e01','830018e180cc0601','87001861806c0600','83001873006c0201','8700183300000001',
   '8200103000000001','c000000000000001','8000000000000001','8000000038000001','800000003c000061','800000003c07c001',
   '86018c000c1fc041','86030e010c78e041','8607060200e07001','860e070c01c07081','8618039c01c07001','863001b801c00001',
   '866000f000e00000','86e000e0003c0002','87f000e000070002','c33800600001c002','c218006000006042','c20c006000003042',
   'c20e006000003002','4207006000003042','4203006000003002','4201806007003002','4201c06003807002','4200c06003c07002',
   '4200606001ffe002','42002060007fc002','40000020001f0002','4000000000000002','4000000000000002','4000000000000002',
   '4000800000140003','4ffffffffffffffe','0000000000000000','0000000000000000']},
  {key:'wide',name:"FRANKY'S breit",w:192,rows:[
   '000000000000000000000000000000000000000000000000','000000000000000000000000000000000000000000000000',
   '000000000000000000000000000000000000000000000000','000000000003fffffffffffffff8000ff800000000000000',
   '06ffffffffffffffffffffffffffffffffffffffffffe000','02c000000007fe00000000000000000ffffffffffffffc00',
   '02c000000000000000000000000000000000000000001c00','02c000000000000000000000000000000000000000001c00',
   '03c000000000000000000000000000000000000000001c00','03c000000000000000000000000000000000000000001c00',
   '03c000000000000000000000000000000000000000000c00','01c000000000000000000000000000000000000000000c00',
   '01c000000000000000000000000000000000078000000e00','01c00000000000080000000000000000000007c000000600',
   '01c007ff8ff000180008000000000000000007e000000600','01c0ffff9ffc001c000c000000000000000007e0003c0300',
   '0143fffe1ffc003c000c000201800080000000e003fc0300','0347ff803ffe003c001e0003018001c1c00000e00ffe0180',
   '034700001c0f003c001e000701800381c000c0e03fbf0180','034700001c07003e001f000f01800781e001c0e07c0f8180',
   '034700001c03803e001f000f01800f00f0038041f0078180','024700001c03803e001f800e01801e00f0070003e007c1c0',
   '024700003c018077001d800e01c03c00780e0003c007c1c0','0247fe003c01c077001d800e01c07800381c0007c007c1c0',
   '0247fe003c01c073801cc00e01c0e0001c3c0007c0038140','0647f8003c01c073801cc00e01c1c0001e780003c0000140',
   '064700003c018073801c600e01c380000ef00001e0000340','064700003c0380e1801c600e01c7000007e00000f0000240',
   '064700003c0380e1c01c701c01cf000007e000007c000240','064700003c0f80e0c01c301c01df000003c000000f800240',
   '064700003fff00e0c01c381c01ff800003c0000001f00640','074700003ffe00e0e01c181c01f3c00003c00000003c04c0',
   '074700003ffc01e0e01c1c1c01e1e00003c00000001f04c0','074700003ff001fff01c0c1c01c0f00001c0000000078cc0',
   '074700003f8001fff01c0e1c01c0f80001c0000000038cc0','034700003fc001c0301c061c01c0780001c000000001c8c0',
   '034700003de001c0381c071c01c03c0001c000000001c9c0','034700003ce003c0181c031801c01e0001c000000000c9c0',
   '034700003cf003801c1c039801c00e0001c000000000d980','034700003c7003801c1c019801c0070001c000000001d980',
   '034700003c7803800e1c01d801c0078001c000000001f380','034700003c3c03800e1c00f801c003c001c0003c0001f300',
   '034700003c3c0380061c00f801c001c001c0003e0003f300','03470000381e0780071c007801c001e001c0001f0003d300',
   '03c70000381e0700071c003801c000f001c0000f8007d300','03c70000380e0700031c003801c0007001c00007c00f9300',
   '03c70000380f0700039c001801c0003801c00003fc7f1300','03c70000380707000118001801c0001801c00001fffe1b00',
   '03e7000038030600000000000080001801c000003ff81b00','01e7000038038600000000000000000001c000000ff00b00',
   '01e000000001000000000000000000000080000000000b00','01e000000000000000000000000000000000000000000f00',
   '01e000000000000000000000000000000000000000000f00','01e000000000000000000000000000000000000000000700',
   '00e000000000000000000000000000000000000000000700','00e000000000000000000000000000000000000000000700',
   '00e000000000000000000000000000000000000000000300','00e000000000000000000000000000000000000000000300',
   '00e000000000000000000000000000003ffc000000000300','00ffffffffffffffff000001fffffff8007ffffffffffb80',
   '0027ffffff80007fffffffffffffffffffffffffffffff80','002000000000000000000000000000000000000000000000',
   '000000000000000000000000000000000000000000000000','000000000000000000000000000000000000000000000000']}
];
const CELL=6;  // editor canvas pixels per LED
let IM={images:[],freeBytes:0,totalBytes:0,frameBytes:W*H*2,maxFrames:8,maxImages:16},thumbs={},ED=null,ICONS=null;
const to565=hex=>{const v=parseInt(hex.slice(1),16),r=v>>16,g=v>>8&255,b=v&255;return (r&0xF8)<<8|(g&0xFC)<<3|b>>3};
const rgbOf=c=>[(c>>11)*255/31|0,(c>>5&63)*255/63|0,(c&31)*255/31|0];
const hexOf=c=>'#'+rgbOf(c).map(v=>v.toString(16).padStart(2,'0')).join('');
// a frame into a 192 x 64 canvas, 1:1, as the panel shows it
function drawSmall(cv,px){
  cv.width=W;cv.height=H;
  const g=cv.getContext('2d'),d=g.createImageData(W,H);
  for(let i=0;i<W*H;i++)d.data.set(ledRGB(px[i])||[11,9,8],i*4),d.data[i*4+3]=255;
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
  const narrow=innerWidth<600;  // on a phone the whole board is too small to draw on
  ED={id:info?id:-1,name:info?info.name:'',delay:info?info.delayMs:300,frames,cur:0,tool:'pen',color:'#ff7800',
    size:1,filled:false,text:'Text',textSize:'normal',icon:'',iconScale:2,tpl:TEMPLATES[0].key,stamp:null,
    zoom:ED?ED.zoom:narrow?4:1,mirror:false,onion:false,undo:[],redo:[],dirty:false,play:0};
  $('editor').hidden=false;$('edMsg').textContent='';
  renderEditor();renderThumbs();
  $('editor').scrollIntoView({behavior:'smooth',block:'start'});
}
function setTool(t){ED.tool=t;renderEditor();if(t==='icon')loadIcons();else restamp()}
// the stamp of the current tool, again (after a change of colour or setting)
function restamp(){if(ED.tool==='text')textStamp();if(ED.tool==='icon')iconStamp();if(ED.tool==='tpl')tplStamp()}
function renderEditor(){
  $('edTitle').textContent=ED.id<0?'Neues Bild':'Bild bearbeiten';
  $('edName').value=ED.name;
  $('edTools').replaceChildren(chips(TOOLS,ED.tool,setTool));
  renderOpts();
  $('edHint').textContent=TOOL_HINTS[ED.tool]+(ED.zoom>1?' Mit zwei Fingern verschieben.':'');
  $('edZoom').replaceChildren(seg(ZOOMS,ED.zoom,v=>{ED.zoom=v;renderEditor()}));
  $('edCanvas').style.width=ED.zoom*100+'%';
  const pal=$('edPal');pal.replaceChildren();
  const pick=c=>{ED.color=c;if(ED.tool==='erase'||ED.tool==='pick')ED.tool='pen';renderEditor();restamp()};
  for(const c of PAL)pal.append(el('button',{type:'button',className:'sw'+(c===ED.color?' on':''),style:'background:'+c,ariaLabel:c,onclick:()=>pick(c)}));
  pal.append(el('input',{type:'color',value:ED.color,title:'eigene Farbe',ariaLabel:'eigene Farbe',className:PAL.includes(ED.color)?'':'sw on',
    onchange:e=>pick(e.target.value)}));
  renderFrames();
  $('edDelay').value=ED.delay;$('edDelayVal').textContent=ED.delay+' ms';
  $('edDelay').disabled=$('edPlay').disabled=ED.frames.length<2;
  $('edAddFrame').disabled=ED.frames.length>=IM.maxFrames;
  $('edDelFrame').disabled=ED.frames.length<2;
  $('edUndo').disabled=!ED.undo.length;
  $('edRedo').disabled=!ED.redo.length;
  $('edMirror').checked=ED.mirror;
  $('edOnion').checked=ED.onion;$('edOnion').disabled=ED.cur===0;
  $('edDelete').hidden=ED.id<0;
  $('edSave').disabled=!ED.dirty;
  drawEditor();
}
// the current tool's settings
function renderOpts(){
  const box=$('edOpts'),t=ED.tool;box.replaceChildren();
  const sizes=[[1,'1'],[2,'2'],[3,'3'],[4,'4']];
  if(['pen','line','rect','circle','erase'].includes(t))box.append(el('span',{className:'flabel',textContent:'Stärke'}),seg(sizes,ED.size,v=>{ED.size=v;renderOpts()}));
  if(t==='rect'||t==='circle')box.append(el('label',{},el('input',{type:'checkbox',checked:ED.filled,onchange:e=>ED.filled=e.target.checked}),' gefüllt'));
  if(t==='text'){
    box.append(el('input',{type:'text',value:ED.text,maxLength:40,placeholder:'Text',ariaLabel:'Text',oninput:e=>{ED.text=e.target.value;textStamp()}}),
      seg(SIZES,ED.textSize,v=>{ED.textSize=v;renderOpts();textStamp()}));
  }
  if(t==='icon'){
    const sel=el('select',{ariaLabel:'Symbol',onchange:e=>{ED.icon=e.target.value;iconStamp()}});
    for(const ic of ICONS||[])sel.append(el('option',{value:ic.key,textContent:ic.name}));
    if(ICONS&&!ICONS.some(ic=>ic.key===ED.icon))ED.icon=ICONS[0]?ICONS[0].key:'';
    sel.value=ED.icon;
    box.append(sel,el('span',{className:'flabel',textContent:'Größe'}),seg([[1,'1×'],[2,'2×'],[3,'3×'],[4,'4×']],ED.iconScale,v=>{ED.iconScale=v;renderOpts();iconStamp()}));
    if(!ICONS)box.append(el('span',{className:'hint',style:'margin:0',textContent:'Lade Symbole ...'}));
  }
  if(t==='tpl')box.append(chips(TEMPLATES.map(tp=>[tp.key,tp.name]),ED.tpl,v=>{ED.tpl=v;renderOpts();tplStamp()}));
  if(t==='text'||t==='icon'||t==='tpl')box.append(el('button',{type:'button',textContent:'In die Mitte',onclick:()=>{
    if(!ED.stamp)return;
    stopPlay();pushUndo();stamp(ED.frames[ED.cur],ED.stamp,{x:W>>1,y:H>>1});markDirty();renderEditor();
  }}));
}
function renderFrames(){
  const box=$('edFrames');box.replaceChildren();
  ED.frames.forEach((f,i)=>{
    const cv=el('canvas',{className:i===ED.cur?'on':'',title:'Einzelbild '+(i+1),onclick:()=>{stopPlay();ED.cur=i;renderEditor()}});
    drawSmall(cv,f);box.append(cv);
  });
}
// the frame 1:1 into a small canvas, scaled up without smoothing, with a grid between the LEDs;
// `ghost` (the previous frame while drawing an animation) shows faintly where the frame is off,
// `cursor` ({x,y,w,h} in LEDs) is outlined where the pointer would draw
const edSmall=el('canvas');
function drawEditor(px=ED.frames[ED.cur],cursor=null){
  drawSmall(edSmall,px);
  const ghost=!ED.play&&ED.onion&&ED.cur>0?ED.frames[ED.cur-1]:null;
  if(ghost){
    const sg=edSmall.getContext('2d'),d=sg.getImageData(0,0,W,H);
    for(let i=0;i<W*H;i++){
      const c=!px[i]&&ledRGB(ghost[i]);
      if(c)for(let k=0;k<3;k++)d.data[i*4+k]=11+(c[k]-11)*.3|0;
    }
    sg.putImageData(d,0,0);
  }
  const g=$('edCanvas').getContext('2d');
  g.imageSmoothingEnabled=false;
  g.drawImage(edSmall,0,0,W*CELL,H*CELL);
  g.fillStyle='#000';
  for(let x=1;x<=W;x++)g.fillRect(x*CELL-1,0,1,H*CELL);
  for(let y=1;y<=H;y++)g.fillRect(0,y*CELL-1,W*CELL,1);
  if(ED.mirror){g.fillStyle='rgba(224,122,31,.7)';g.fillRect(W/2*CELL-2,0,3,H*CELL)}
  if(cursor){
    g.strokeStyle='rgba(255,255,255,.8)';g.lineWidth=2;
    const box=c=>g.strokeRect(c.x*CELL,c.y*CELL,c.w*CELL-1,c.h*CELL-1);
    box(cursor);if(ED.mirror&&ED.tool!=='text'&&ED.tool!=='icon'&&ED.tool!=='tpl')box({...cursor,x:W-cursor.x-cursor.w});
  }
}
function markDirty(){ED.dirty=true;$('edSave').disabled=false;$('edUndo').disabled=!ED.undo.length;$('edRedo').disabled=!ED.redo.length}
const snapshot=()=>({cur:ED.cur,frames:ED.frames.map(f=>f.slice())});
function pushUndo(){ED.undo.push(snapshot());ED.redo=[];if(ED.undo.length>30)ED.undo.shift()}
// takes the last state from `from`, the current one goes onto `to`
function undoRedo(from,to){
  const u=from.pop();if(!u)return;
  stopPlay();to.push(snapshot());
  ED.frames=u.frames;ED.cur=Math.min(u.cur,u.frames.length-1);markDirty();renderEditor();
}
$('edUndo').onclick=()=>undoRedo(ED.undo,ED.redo);
$('edRedo').onclick=()=>undoRedo(ED.redo,ED.undo);
document.addEventListener('keydown',e=>{
  if(!ED||$('editor').hidden||!(e.ctrlKey||e.metaKey)||e.key.toLowerCase()!=='z'||e.target.matches('input[type=text]'))return;
  e.preventDefault();
  if(e.shiftKey)undoRedo(ED.redo,ED.undo);else undoRedo(ED.undo,ED.redo);
});
$('edClear').onclick=()=>{stopPlay();pushUndo();ED.frames[ED.cur].fill(0);markDirty();renderEditor()};
$('edName').oninput=e=>{ED.name=e.target.value;markDirty()};
$('edMirror').onchange=e=>{ED.mirror=e.target.checked;drawEditor()};
$('edOnion').onchange=e=>{ED.onion=e.target.checked;drawEditor()};
// moves the current frame by one LED (what goes over the edge is lost), or turns it round
function shift(dx,dy){
  stopPlay();pushUndo();
  const f=ED.frames[ED.cur],old=f.slice();f.fill(0);
  for(let y=0;y<H;y++)for(let x=0;x<W;x++){
    const X=x+dx,Y=y+dy;
    if(X>=0&&X<W&&Y>=0&&Y<H)f[Y*W+X]=old[y*W+x];
  }
  markDirty();renderEditor();
}
$('edLeft').onclick=()=>shift(-1,0);$('edRight').onclick=()=>shift(1,0);
$('edUp').onclick=()=>shift(0,-1);$('edDown').onclick=()=>shift(0,1);
$('edFlip').onclick=()=>{
  stopPlay();pushUndo();
  const f=ED.frames[ED.cur];
  for(let y=0;y<H;y++)f.subarray(y*W,(y+1)*W).reverse();
  markDirty();renderEditor();
};

// ---- shapes on a frame `f` (value v = RGB565, 0 = off); with Spiegeln
// also mirrored onto the other half ----
function dot(f,x,y,v,size=1){
  const o=Math.floor((size-1)/2);
  for(let dy=0;dy<size;dy++)for(let dx=0;dx<size;dx++){
    const px=x-o+dx,py=y-o+dy;
    if(px>=0&&px<W&&py>=0&&py<H){f[py*W+px]=v;if(ED.mirror)f[py*W+W-1-px]=v}
  }
}
function line(f,a,b,v,size){
  const n=Math.max(Math.abs(b.x-a.x),Math.abs(b.y-a.y))||1;
  for(let k=0;k<=n;k++)dot(f,Math.round(a.x+(b.x-a.x)*k/n),Math.round(a.y+(b.y-a.y)*k/n),v,size);
}
// horizontal, vertical or diagonal if the direction is close to it
function snap(a,b){
  const dx=b.x-a.x,dy=b.y-a.y,ang=Math.atan2(dy,dx),s=Math.round(ang/(Math.PI/4))*(Math.PI/4);
  if(Math.abs(ang-s)>0.16)return b;
  const k=Math.round(s/(Math.PI/4))&3;  // 0: horizontal, 2: vertical, 1/3: diagonal
  if(k===0)return {x:b.x,y:a.y};
  if(k===2)return {x:a.x,y:b.y};
  const d=Math.round((Math.abs(dx)+Math.abs(dy))/2);
  return {x:a.x+Math.sign(dx)*d,y:a.y+Math.sign(dy)*d};
}
function rect(f,a,b,v,size,filled){
  const x0=Math.min(a.x,b.x),x1=Math.max(a.x,b.x),y0=Math.min(a.y,b.y),y1=Math.max(a.y,b.y);
  if(filled){for(let y=y0;y<=y1;y++)for(let x=x0;x<=x1;x++)dot(f,x,y,v);return}
  line(f,{x:x0,y:y0},{x:x1,y:y0},v,size);line(f,{x:x1,y:y0},{x:x1,y:y1},v,size);
  line(f,{x:x1,y:y1},{x:x0,y:y1},v,size);line(f,{x:x0,y:y1},{x:x0,y:y0},v,size);
}
function circle(f,c,p,v,size,filled){
  const r=Math.round(Math.hypot(p.x-c.x,p.y-c.y));
  for(let y=-r-1;y<=r+1;y++)for(let x=-r-1;x<=r+1;x++){
    const d=Math.hypot(x,y);
    if(filled?d<=r+.3:Math.abs(d-r)<.5)dot(f,c.x+x,c.y+y,v,filled?1:size);
  }
}
function fill(f,p,v){
  const from=f[p.y*W+p.x];if(from===v)return;
  const stack=[p.y*W+p.x];
  while(stack.length){
    const i=stack.pop();if(f[i]!==from)continue;f[i]=v;
    const x=i%W;
    if(x>0)stack.push(i-1);if(x<W-1)stack.push(i+1);if(i>=W)stack.push(i-W);if(i<W*(H-1))stack.push(i+W);
  }
}
// a stamp (text or symbol) centred on p; its 0 pixels leave the picture as it is
function stamp(f,s,p){
  const x0=p.x-(s.w>>1),y0=p.y-(s.h>>1);
  for(let y=0;y<s.h;y++)for(let x=0;x<s.w;x++){
    const v=s.px[y*s.w+x],X=x0+x,Y=y0+y;
    if(v&&X>=0&&X<W&&Y>=0&&Y<H)f[Y*W+X]=v;
  }
}
// The text as the board writes it: it draws it itself (/api/event/preview),
// then only the part with pixels is kept.
let stampSeq=0;
async function textStamp(){
  const seq=++stampSeq;ED.stamp=null;
  if(!ED.text.trim())return;
  await new Promise(r=>setTimeout(r,200));
  if(seq!==stampSeq)return;
  try{
    const sc={name:'',durS:8,bg:'#000000',lines:[{text:ED.text,color:ED.color,size:ED.textSize,v:'top',h:'left',anim:'none',speed:5}]};
    const r=await fetch('/api/event/preview?ms=0',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(sc)});
    if(!r.ok||seq!==stampSeq)return;
    const runs=new Uint16Array(await r.arrayBuffer()),px=new Uint16Array(W*H);
    for(let i=0,p=0;i+1<runs.length&&p<px.length;i+=2){px.fill(runs[i+1],p,p+runs[i]);p+=runs[i]}
    let x0=W,y0=H,x1=-1,y1=-1;
    for(let i=0;i<W*H;i++)if(px[i]){const x=i%W,y=i/W|0;x0=Math.min(x0,x);x1=Math.max(x1,x);y0=Math.min(y0,y);y1=Math.max(y1,y)}
    if(x1<0)return;
    const w=x1-x0+1,h=y1-y0+1,s=new Uint16Array(w*h);
    for(let y=0;y<h;y++)s.set(px.subarray((y0+y)*W+x0,(y0+y)*W+x0+w),y*w);
    ED.stamp={w,h,px:s};
  }catch(e){}
}
// the promo icons (built-in and own ones) as symbols
async function loadIcons(){
  if(ICONS)return iconStamp();
  try{
    const s=await (await fetch('/api/settings',{cache:'no-store'})).json();
    ICONS=s.presetIcons.map(i=>({key:i.tag,name:i.name,rows:i.rows.map(r=>[...r].map(c=>c==='#'))}))
      .concat(s.icons.map(i=>({key:'c'+i.id,name:(i.name||'Icon '+(i.id+1))+' (eigenes)',rows:i.rows.map(r=>[...Array(12)].map((_,x)=>!!(r>>(11-x)&1)))})));
  }catch(e){ICONS=[]}
  if(ED&&ED.tool==='icon')renderOpts();
  iconStamp();
}
function iconStamp(){
  const ic=ICONS&&ICONS.find(i=>i.key===ED.icon);ED.stamp=null;
  if(!ic)return;
  const k=ED.iconScale,h=ic.rows.length,w=ic.rows[0].length,v=to565(ED.color),s=new Uint16Array(w*k*h*k);
  for(let y=0;y<h*k;y++)for(let x=0;x<w*k;x++)if(ic.rows[y/k|0][x/k|0])s[y*w*k+x]=v;
  ED.stamp={w:w*k,h:h*k,px:s};
}
function tplStamp(){
  const tp=TEMPLATES.find(t=>t.key===ED.tpl),v=to565(ED.color),w=tp.w,h=tp.rows.length,s=new Uint16Array(w*h);
  tp.rows.forEach((r,y)=>{for(let x=0;x<w;x++)if(parseInt(r[x>>2],16)>>(3-(x&3))&1)s[y*w+x]=v});
  ED.stamp={w,h,px:s};
}

// ---- pointer: one finger (or the mouse) draws, two fingers move the zoomed picture ----
const cv=$('edCanvas'),pointers=new Map();
let stroke=null,panning=false,lastMid=null;
function cellAt(e){
  const r=cv.getBoundingClientRect();
  return {x:Math.min(W-1,Math.max(0,Math.floor((e.clientX-r.left)/r.width*W))),y:Math.min(H-1,Math.max(0,Math.floor((e.clientY-r.top)/r.height*H)))};
}
function mid(){const p=[...pointers.values()];return {x:(p[0].x+p[1].x)/2,y:(p[0].y+p[1].y)/2}}
function cancelStroke(){if(!stroke)return;ED.frames[ED.cur].set(stroke.snap);ED.undo.pop();stroke=null;drawEditor()}
// draws the stroke's shape from its start to p onto the picture as it was before it
function strokeTo(p){
  const f=ED.frames[ED.cur],t=stroke.tool,v=t==='erase'?0:to565(ED.color);
  if(t==='pen'||t==='erase'){line(f,stroke.last,p,v,ED.size);stroke.last=p;drawEditor();return}
  f.set(stroke.snap);
  if(t==='line')line(f,stroke.start,snap(stroke.start,p),v,ED.size);
  else if(t==='rect')rect(f,stroke.start,p,v,ED.size,ED.filled);
  else if(t==='circle')circle(f,stroke.start,p,v,ED.size,ED.filled);
  else if(['text','icon','tpl'].includes(t)&&ED.stamp)stamp(f,ED.stamp,p);
  drawEditor();
}
cv.onpointerdown=e=>{
  if(!ED)return;
  e.preventDefault();stopPlay();
  pointers.set(e.pointerId,{x:e.clientX,y:e.clientY});
  if(pointers.size===2){cancelStroke();panning=true;lastMid=mid();return}
  if(pointers.size>2||panning)return;
  try{cv.setPointerCapture(e.pointerId)}catch(err){}
  const p=cellAt(e),f=ED.frames[ED.cur],c=f[p.y*W+p.x];
  if(ED.tool==='pick'){if(c){ED.color=hexOf(c);ED.tool='pen';renderEditor()}return}
  if(['text','icon','tpl'].includes(ED.tool)&&!ED.stamp){$('edMsg').textContent=ED.tool==='text'?'Erst einen Text eingeben.':'Erst ein Symbol wählen.';return}
  pushUndo();
  if(ED.tool==='fill'){fill(f,p,to565(ED.color));if(ED.mirror)fill(f,{x:W-1-p.x,y:p.y},to565(ED.color));markDirty();renderEditor();return}
  stroke={tool:ED.tool,start:p,last:p,snap:f.slice()};
  strokeTo(p);
};
cv.onpointermove=e=>{
  if(!pointers.has(e.pointerId))return;
  pointers.set(e.pointerId,{x:e.clientX,y:e.clientY});
  if(panning){
    if(pointers.size<2)return;
    const m=mid(),w=$('edWrap');
    w.scrollLeft-=m.x-lastMid.x;w.scrollTop-=m.y-lastMid.y;lastMid=m;
    return;
  }
  if(stroke)strokeTo(cellAt(e));
};
// with a mouse, before pressing: where it would draw (a stamp as it would land)
cv.addEventListener('pointermove',e=>{
  if(!ED||e.pointerType!=='mouse'||e.buttons||stroke||ED.play)return;
  const p=cellAt(e),t=ED.tool;
  if(['text','icon','tpl'].includes(t)){
    if(!ED.stamp)return drawEditor();
    const f=ED.frames[ED.cur].slice();stamp(f,ED.stamp,p);
    drawEditor(f,{x:p.x-(ED.stamp.w>>1),y:p.y-(ED.stamp.h>>1),w:ED.stamp.w,h:ED.stamp.h});
    return;
  }
  const n=['pen','line','rect','circle','erase'].includes(t)?ED.size:1,o=Math.floor((n-1)/2);
  drawEditor(undefined,{x:p.x-o,y:p.y-o,w:n,h:n});
});
cv.addEventListener('pointerleave',()=>{if(ED&&!stroke&&!ED.play)drawEditor()});
cv.onpointerup=cv.onpointercancel=e=>{
  pointers.delete(e.pointerId);
  if(panning){if(!pointers.size)panning=false;return}
  if(stroke){stroke=null;markDirty();renderFrames()}
};

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
