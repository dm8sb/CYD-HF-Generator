// Screenshots over the Chrome DevTools protocol for tools/render_images.py (Node 22+, no
// packages: the global WebSocket). Starts headless Chrome/Edge with its own temporary profile,
// emulates the viewport (also a phone), waits for a condition, optionally scrolls an element to
// the top or clips to it, and writes a PNG.
//
//   node tools/cdp_shot.mjs <chrome> <spec.json>
//
// spec: [{"url", "out", "width", "height", "scale", "mobile", "waitFor": "<JS expression>",
//         "scrollTo": "<selector>", "clip": "<selector>", "clipPad": px, "maxHeight": px}]
import { spawn } from 'node:child_process';
import { mkdtempSync, readFileSync, rmSync, writeFileSync, existsSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';

const [chrome, specPath] = process.argv.slice(2);
const shots = JSON.parse(readFileSync(specPath, 'utf8'));
const profile = mkdtempSync(join(tmpdir(), 'cdp-shot-'));
const proc = spawn(chrome, ['--headless=new', '--disable-gpu', '--hide-scrollbars', '--no-first-run',
  '--no-default-browser-check', '--disable-extensions', '--remote-debugging-port=0',
  '--user-data-dir=' + profile, 'about:blank'], {stdio: 'ignore'});

const sleep = ms => new Promise(r => setTimeout(r, ms));

async function browserUrl() {
  const file = join(profile, 'DevToolsActivePort');
  for (let i = 0; i < 200; i++) {
    if (existsSync(file)) {
      const [port, path] = readFileSync(file, 'utf8').split('\n');
      if (path) return `ws://127.0.0.1:${port.trim()}${path.trim()}`;
    }
    await sleep(50);
  }
  throw new Error('Chrome did not start');
}

let ws, nextId = 1;
const pending = new Map();
function send(method, params = {}, sessionId) {
  const id = nextId++;
  ws.send(JSON.stringify({id, method, params, sessionId}));
  return new Promise((resolve, reject) => pending.set(id, {resolve, reject}));
}

async function evaluate(session, expression) {
  const r = await send('Runtime.evaluate', {expression, returnByValue: true, awaitPromise: true}, session);
  if (r.exceptionDetails) throw new Error('evaluate: ' + expression);
  return r.result.value;
}

async function shoot(s) {
  const {targetId} = await send('Target.createTarget', {url: 'about:blank'});
  const {sessionId} = await send('Target.attachToTarget', {targetId, flatten: true});
  await send('Emulation.setDeviceMetricsOverride', {width: s.width, height: s.height,
    deviceScaleFactor: s.scale || 1, mobile: !!s.mobile}, sessionId);
  if (s.mobile) await send('Emulation.setTouchEmulationEnabled', {enabled: true}, sessionId);
  await send('Page.enable', {}, sessionId);
  await send('Page.navigate', {url: s.url}, sessionId);
  const until = Date.now() + 15000;
  while (!(await evaluate(sessionId, `document.readyState === 'complete' && (${s.waitFor || 'true'})`))) {
    if (Date.now() > until) throw new Error('timeout waiting for ' + (s.waitFor || 'load') + ' on ' + s.url);
    await sleep(100);
  }
  if (s.scrollTo) {
    await evaluate(sessionId, `document.querySelector(${JSON.stringify(s.scrollTo)}).scrollIntoView({block: 'start'})`);
  }
  await sleep(s.settleMs || 600);
  const params = {format: 'png'};
  if (s.clip) {
    const r = await evaluate(sessionId, `(() => { const b = document.querySelector(${JSON.stringify(s.clip)})
      .getBoundingClientRect(); return {x: b.left + scrollX, y: b.top + scrollY, w: b.width, h: b.height}; })()`);
    const pad = s.clipPad || 0;
    const h = Math.min(r.h + 2 * pad, s.maxHeight || 1e9);
    params.clip = {x: r.x - pad, y: r.y - pad, width: r.w + 2 * pad, height: h, scale: 1};
    params.captureBeyondViewport = true;
  }
  const {data} = await send('Page.captureScreenshot', params, sessionId);
  writeFileSync(s.out, Buffer.from(data, 'base64'));
  await send('Target.closeTarget', {targetId});
  console.log(s.out);
}

try {
  ws = new WebSocket(await browserUrl());
  await new Promise((resolve, reject) => { ws.onopen = resolve; ws.onerror = reject; });
  ws.onmessage = ev => {
    const m = JSON.parse(ev.data);
    const p = m.id && pending.get(m.id);
    if (!p) return;
    pending.delete(m.id);
    if (m.error) p.reject(new Error(m.error.message));
    else p.resolve(m.result);
  };
  for (const s of shots) await shoot(s);
  ws.close();
} catch (e) {
  console.error(e.message);
  process.exitCode = 1;
} finally {
  proc.kill();
  await sleep(300);
  try { rmSync(profile, {recursive: true, force: true}); } catch { /* Chrome still holds files */ }
}
