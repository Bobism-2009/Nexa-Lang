// Drives a --wasm page in headless Chrome for Tests/wasm_run_cases.sh.
//
//   node wasm_browser.mjs <chrome> <page.html> <actions.json> [--serve <port>]
//
// Prints what the page said, one line each: console output ("console.log: ..."),
// exceptions ("EXCEPTION: ..."), and the value of every "eval" action. --serve
// starts an HTTP server on 127.0.0.1:<port> for the page to call: it answers
// "<METHOD> <path> [<body>] <X-T header>", 404 for /missing, with CORS allowed
// (the page is a file:// origin).
//
// Actions, each a JSON array:
//   ["wait", ms]   ["move", x, y]   ["down", x, y]   ["up", x, y]
//   ["wheel", x, y, deltaY]   ["keydown", key, code, keyCode, text]
//   ["keyup", key, code, keyCode]   ["eval", "js expression"]
//   ["canvas"]   moves the origin of later x, y to the canvas's top left
import { spawn } from 'node:child_process';
import { mkdtempSync, readFileSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join, resolve } from 'node:path';
import http from 'node:http';

const [, , chromePath, page, actionsFile, serveFlag, servePort] = process.argv;
const actions = JSON.parse(readFileSync(actionsFile, 'utf8'));
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

let server = null;
if (serveFlag === '--serve') {
  server = http.createServer((req, res) => {
    let body = '';
    req.on('data', (c) => { body += c; });
    req.on('end', () => {
      res.writeHead(req.url === '/missing' ? 404 : 200, {
        'Access-Control-Allow-Origin': '*', 'Access-Control-Allow-Headers': '*',
        'Access-Control-Allow-Methods': '*' });
      res.end(req.method === 'OPTIONS' ? '' : `${req.method} ${req.url} [${body}] ${req.headers['x-t'] || ''}`);
    });
  }).listen(Number(servePort), '127.0.0.1');
}

const port = 9300 + Math.floor(Math.random() * 600);
const prof = mkdtempSync(join(tmpdir(), 'nexa-chrome-'));
const chrome = spawn(chromePath, ['--headless=new', `--remote-debugging-port=${port}`,
  `--user-data-dir=${prof}`, '--allow-file-access-from-files', '--no-first-run',
  '--autoplay-policy=no-user-gesture-required', '--window-size=800,600', 'about:blank'],
  { stdio: 'ignore' });
const done = (code) => {
  try { chrome.kill(); } catch {}
  if (server) server.close();
  setTimeout(() => { try { rmSync(prof, { recursive: true, force: true }); } catch {} process.exit(code); }, 300);
};
setTimeout(() => { console.log('TIMEOUT'); done(1); }, 60000);

let targets = null;
for (let i = 0; i < 150 && !targets; i++) {
  try { targets = await (await fetch(`http://127.0.0.1:${port}/json`)).json(); } catch { await sleep(100); }
}
if (!targets) { console.log('NO CHROME'); done(1); }
const ws = new WebSocket(targets.find((t) => t.type === 'page').webSocketDebuggerUrl);
await new Promise((r) => ws.addEventListener('open', r));
let id = 0;
const pending = new Map();
ws.addEventListener('message', (ev) => {
  const m = JSON.parse(ev.data);
  if (m.id && pending.has(m.id)) { pending.get(m.id)(m); pending.delete(m.id); return; }
  if (m.method === 'Runtime.consoleAPICalled') {
    console.log(`console.${m.params.type}: ` + m.params.args.map((a) => a.value ?? a.description).join(' '));
  }
  if (m.method === 'Runtime.exceptionThrown') {
    const d = m.params.exceptionDetails;
    console.log('EXCEPTION: ' + (d.exception?.description || d.text).split('\n')[0]);
  }
});
const send = (method, params = {}) => new Promise((r) => {
  const i = ++id; pending.set(i, r); ws.send(JSON.stringify({ id: i, method, params }));
});
await send('Runtime.enable');
await send('Page.enable');
await send('Page.navigate', { url: 'file:///' + resolve(page).replace(/\\/g, '/').replace(/^\//, '') });

let ox = 0, oy = 0;
for (const [op, ...a] of actions) {
  if (op === 'wait') await sleep(a[0]);
  else if (op === 'canvas') {
    const r = await send('Runtime.evaluate', { expression:
      'JSON.stringify(document.querySelector("canvas").getBoundingClientRect())', returnByValue: true });
    const b = JSON.parse(r.result.result.value); ox = b.x; oy = b.y;
  } else if (op === 'move' || op === 'down' || op === 'up') {
    await send('Input.dispatchMouseEvent', {
      type: { move: 'mouseMoved', down: 'mousePressed', up: 'mouseReleased' }[op],
      x: ox + a[0], y: oy + a[1], button: op === 'move' ? 'none' : 'left',
      buttons: op === 'down' ? 1 : 0, clickCount: 1 });
  } else if (op === 'wheel') {
    await send('Input.dispatchMouseEvent', { type: 'mouseWheel', x: ox + a[0], y: oy + a[1], deltaX: 0, deltaY: a[2] });
  } else if (op === 'keydown' || op === 'keyup') {
    const [key, code, kc, text] = a;
    await send('Input.dispatchKeyEvent', {
      type: op === 'keyup' ? 'keyUp' : (text ? 'keyDown' : 'rawKeyDown'),
      key, code, windowsVirtualKeyCode: kc, text: op === 'keyup' ? undefined : text });
  } else if (op === 'eval') {
    const r = await send('Runtime.evaluate', { expression: a[0], returnByValue: true, awaitPromise: true });
    console.log('eval: ' + JSON.stringify(r.result?.result?.value ?? null));
  }
}
ws.close();
done(0);
