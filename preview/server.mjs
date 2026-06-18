/* preview/server.mjs — KN-86 screen DSL live viewer.
 *
 * A thin web VIEWER (not a renderer): it shells out to build/tools/render_screen
 * — the real C engine + font.c — to render a screen .lsp to the actual RGB565
 * surface, converts that to PNG, and shows it in the browser. The image IS the
 * device-faithful render; the browser only displays + edits the DSL.
 *
 * Live loop: edit a screen .lsp (in the browser editor, or from the terminal)
 * → fs.watch fires → SSE → the browser re-fetches the image. Both sides see it.
 *
 *   node preview/server.mjs   →   http://127.0.0.1:7787
 */
import http from 'node:http';
import {readFileSync, writeFileSync, readdirSync, watch, mkdtempSync, cpSync} from 'node:fs';
import {execFileSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
import {dirname, join, basename} from 'node:path';
import {tmpdir} from 'node:os';

const HERE = dirname(fileURLToPath(import.meta.url));
const ROOT = dirname(HERE);
const SCREENS_DIR = join(ROOT, 'examples');
const UI_DIR = join(ROOT, 'ui');
const STAGED_UI = join(ROOT, 'build', 'lib', 'ui');   // render_screen loads the kit from here
const BIN = join(ROOT, 'build', 'tools', 'render_screen');
const TMP = mkdtempSync(join(tmpdir(), 'kn86prev-'));
const PORT = 7787;

const lastGood = new Map(); /* screen -> png Buffer */

function listScreens() {
  return readdirSync(SCREENS_DIR).filter(f => f.endsWith('.lsp')).sort();
}

/* render a screen file to PNG via the real engine; throws with stderr on failure */
function renderPng(screen) {
  cpSync(UI_DIR, STAGED_UI, {recursive: true});  // sync engine + kit edits into the staged copy
  const src = join(SCREENS_DIR, screen);
  const ppm = join(TMP, screen + '.ppm');
  const png = join(TMP, screen + '.png');
  execFileSync(BIN, [src, ppm], {stdio: ['ignore', 'ignore', 'pipe']});
  execFileSync('python3', ['-c',
    `from PIL import Image; Image.open(${JSON.stringify(ppm)}).save(${JSON.stringify(png)})`]);
  const buf = readFileSync(png);
  lastGood.set(screen, buf);
  return buf;
}

/* ---- SSE ---- */
const clients = new Set();
function broadcast(ev, data = 'stale') {
  for (const r of clients) r.write(`event: ${ev}\ndata: ${data}\n\n`);
}
let debounce = null;
function onChange() {
  clearTimeout(debounce);
  debounce = setTimeout(() => broadcast('stale'), 80);
}
watch(SCREENS_DIR, onChange);
watch(UI_DIR, onChange);   // engine / kit edits live-refresh too

function send(res, code, type, body) {
  res.writeHead(code, {'Content-Type': type, 'Cache-Control': 'no-store'});
  res.end(body);
}
function readBody(req) {
  return new Promise(r => { let b = ''; req.on('data', c => b += c); req.on('end', () => r(b)); });
}

const server = http.createServer(async (req, res) => {
  const url = new URL(req.url, `http://${req.headers.host}`);
  const p = url.pathname;

  if (p === '/events') {
    res.writeHead(200, {'Content-Type': 'text/event-stream', 'Cache-Control': 'no-cache', Connection: 'keep-alive'});
    res.write('retry: 2000\n\n');
    clients.add(res);
    req.on('close', () => clients.delete(res));
    return;
  }

  if (p === '/screens') return send(res, 200, 'application/json', JSON.stringify(listScreens()));

  if (p === '/source') {
    const s = url.searchParams.get('s');
    try { return send(res, 200, 'text/plain; charset=utf-8', readFileSync(join(SCREENS_DIR, basename(s)), 'utf8')); }
    catch { return send(res, 404, 'text/plain', ''); }
  }

  if (p === '/render') {
    const s = basename(url.searchParams.get('s') || '');
    try {
      return send(res, 200, 'image/png', renderPng(s));
    } catch (e) {
      const msg = (e.stderr ? e.stderr.toString() : e.message).slice(-800);
      res.writeHead(200, {'Content-Type': 'application/json', 'Cache-Control': 'no-store'});
      return res.end(JSON.stringify({error: msg}));
    }
  }

  if (p === '/save' && req.method === 'POST') {
    const body = JSON.parse(await readBody(req));
    writeFileSync(join(SCREENS_DIR, basename(body.s)), body.text);
    return send(res, 204, 'text/plain', ''); /* fs.watch -> SSE */
  }

  if (p === '/') return send(res, 200, 'text/html', SHELL);
  return send(res, 404, 'text/plain', 'not found');
});

server.listen(PORT, '127.0.0.1', () => console.log(`KN-86 screen viewer → http://127.0.0.1:${PORT}`));

const SHELL = /* html */ `<!doctype html><html><head><meta charset="utf-8">
<title>KN-86 Screen Viewer</title>
<link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/codemirror/5.65.16/codemirror.min.css">
<style>
 :root{--amber:#E6A020;--dim:rgba(230,160,32,.18);--warm:#9c9689;--off:#e8e4df;
   --mono:'JetBrains Mono',ui-monospace,Menlo,monospace}
 *{box-sizing:border-box} body{margin:0;background:#000;color:var(--off);font-family:var(--mono);
   font-size:13px;display:grid;grid-template-columns:200px 1fr 420px;height:100vh;overflow:hidden}
 .col{border-right:1px solid var(--dim);overflow:auto;padding:12px}
 h2{font-size:10px;letter-spacing:.16em;color:var(--amber);text-transform:uppercase;margin:0 0 10px}
 .rec{display:block;width:100%;text-align:left;background:transparent;border:1px solid var(--dim);
   color:var(--off);font-family:var(--mono);font-size:12px;padding:7px 9px;margin-bottom:6px;cursor:pointer;border-radius:0}
 .rec.on{border-color:var(--amber);color:var(--amber)}
 .rec:hover{border-color:var(--amber)}
 .stage{display:flex;flex-direction:column;align-items:center;justify-content:center;padding:18px;
   background:radial-gradient(ellipse at center,rgba(230, 160, 32,.06),#000 70%);overflow:auto}
 .device{border:1px solid var(--dim);background:#000;box-shadow:0 0 40px rgba(230, 160, 32,.10);position:relative;line-height:0;width:100%;max-width:1024px}
 .device img{display:block;width:100%;height:auto;image-rendering:pixelated}
 .device::after{content:"";position:absolute;inset:0;pointer-events:none;
   background:repeating-linear-gradient(to bottom,rgba(0,0,0,0) 0 2px,rgba(0,0,0,.16) 2px 3px)}
 .cap{color:var(--warm);font-size:11px;letter-spacing:.08em;margin-top:10px;text-transform:uppercase}
 .editor{grid-column:3;border-left:1px solid var(--dim);display:flex;flex-direction:column;padding:12px;overflow:hidden}
 textarea{flex:1;background:#0a0a08;color:var(--off);border:1px solid var(--dim);border-radius:0;
   font-family:var(--mono);font-size:12px;padding:10px;resize:none;line-height:1.5;tab-size:2}
 .bar{display:flex;gap:8px;margin-top:8px;align-items:center}
 button.save{background:transparent;border:1px solid var(--amber);color:var(--amber);padding:7px 14px;cursor:pointer;border-radius:0;font-family:var(--mono)}
 .err{color:#ff6a4a;font-size:11px;white-space:pre-wrap;margin-top:8px;max-height:120px;overflow:auto}
 .ok{color:var(--warm);font-size:11px}
 ::-webkit-scrollbar{width:8px;height:8px}::-webkit-scrollbar-thumb{background:var(--dim)}
 /* CodeMirror — amber/black KEC theme */
 .editor .CodeMirror{flex:1 1 auto;min-height:0;height:auto;border:1px solid var(--dim);
   font-family:var(--mono);font-size:12px;line-height:1.55}
 .cm-s-kec.CodeMirror{background:#0a0a08;color:#e8e4df}
 .cm-s-kec .CodeMirror-gutters{background:#0a0a08;border-right:1px solid var(--dim)}
 .cm-s-kec .CodeMirror-linenumber{color:#5c574a}
 .cm-s-kec .CodeMirror-cursor{border-left:2px solid var(--amber)}
 .cm-s-kec .CodeMirror-activeline-background{background:rgba(230,160,32,.05)}
 .cm-s-kec .CodeMirror-selected,.cm-s-kec .CodeMirror-line::selection{background:rgba(230,160,32,.18)}
 .cm-s-kec .cm-comment{color:#9c9689;font-style:italic}
 .cm-s-kec .cm-string{color:#ffc040}
 .cm-s-kec .cm-number{color:#e6a020}
 .cm-s-kec .cm-keyword,.cm-s-kec .cm-builtin,.cm-s-kec .cm-atom{color:#e6a020}
 .cm-s-kec .cm-variable,.cm-s-kec .cm-variable-2,.cm-s-kec .cm-def,.cm-s-kec .cm-property{color:#e8e4df}
 .cm-s-kec .cm-bracket{color:#c9a35a}
 .cm-s-kec .CodeMirror-matchingbracket{color:#000 !important;background:var(--amber);font-weight:700}
 .cm-s-kec .CodeMirror-nonmatchingbracket{color:#ff6a4a !important}
</style></head><body>
 <div class="col"><h2>Screens</h2><div id="list"></div></div>
 <div class="stage">
   <div class="device"><img id="shot" alt="render"></div>
   <div class="cap" id="cap">—</div>
 </div>
 <div class="editor">
   <h2>DSL — <span id="ename" style="color:var(--off)"></span></h2>
   <textarea id="src" spellcheck="false"></textarea>
   <div class="bar"><button class="save" onclick="save()">SAVE (⌘S)</button><button class="save" onclick="format()">FORMAT (⇧⌘F)</button><span id="status" class="ok"></span></div>
   <div class="err" id="err"></div>
 </div>
<script src="https://cdnjs.cloudflare.com/ajax/libs/codemirror/5.65.16/codemirror.min.js"></script>
<script src="https://cdnjs.cloudflare.com/ajax/libs/codemirror/5.65.16/mode/commonlisp/commonlisp.min.js"></script>
<script src="https://cdnjs.cloudflare.com/ajax/libs/codemirror/5.65.16/addon/edit/matchbrackets.min.js"></script>
<script src="https://cdnjs.cloudflare.com/ajax/libs/codemirror/5.65.16/addon/edit/closebrackets.min.js"></script>
<script src="https://cdnjs.cloudflare.com/ajax/libs/codemirror/5.65.16/addon/selection/active-line.min.js"></script>
<script>
 const cm = CodeMirror.fromTextArea(document.getElementById('src'), {
   mode:'commonlisp', theme:'kec', lineNumbers:true, matchBrackets:true,
   autoCloseBrackets:true, styleActiveLine:true, lineWrapping:true,
   extraKeys:{'Cmd-S':()=>save(),'Ctrl-S':()=>save(),
              'Shift-Cmd-F':()=>format(),'Shift-Ctrl-F':()=>format()}});
 let sel=null, screens=[];
 const shot=document.getElementById('shot');
 async function loadList(){
   screens=await (await fetch('/screens')).json();
   const L=document.getElementById('list'); L.innerHTML='';
   for(const s of screens){
     const b=document.createElement('button'); b.className='rec'+(s===sel?' on':''); b.textContent=s;
     b.onclick=()=>select(s); L.appendChild(b);
   }
 }
 async function select(s){
   sel=s; document.getElementById('ename').textContent=s;
   const t=await (await fetch('/source?s='+encodeURIComponent(s))).text();
   cm.setValue(t); cm.clearHistory();
   await loadList(); refresh();
 }
 async function refresh(){
   if(!sel) return;
   document.getElementById('cap').textContent='rendering…';
   // detect render error (json) vs png
   const r=await fetch('/render?s='+encodeURIComponent(sel)+'&t='+Date.now());
   if(r.headers.get('content-type').startsWith('application/json')){
     const {error}=await r.json();
     document.getElementById('err').textContent=error||'render error';
     document.getElementById('cap').textContent='⚠ render error (showing last good)';
   }else{
     document.getElementById('err').textContent='';
     shot.src='/render?s='+encodeURIComponent(sel)+'&t='+Date.now();
     document.getElementById('cap').textContent=sel+'  ·  real render · 1024×600';
   }
 }
 async function save(){
   if(!sel) return;
   await fetch('/save',{method:'POST',headers:{'Content-Type':'application/json'},
     body:JSON.stringify({s:sel,text:cm.getValue()})});
   const st=document.getElementById('status'); st.textContent='saved'; setTimeout(()=>st.textContent='',1500);
 }
 function format(){ cm.operation(()=>{ for(let i=0;i<cm.lineCount();i++) cm.indentLine(i,'smart'); }); }
 document.addEventListener('keydown',e=>{if((e.metaKey||e.ctrlKey)&&e.key==='s'){e.preventDefault();save();}});
 const ev=new EventSource('/events');
 ev.addEventListener('stale',()=>refresh());
 (async()=>{ await loadList(); if(screens.length) select(screens[0]); })();
</script></body></html>`;
