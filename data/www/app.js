// CYD HF Generator web UI. No framework; device text only via textContent (W-S8).
'use strict';
(() => {
const D = document;
const $ = (id) => D.getElementById(id);
const TOKEN = (D.querySelector('meta[name="cyd-token"]') || {}).content || '';
const FMIN = 8000, FMAX = 160000000, CMAX = 5000;
const STEPS = [1000000, 100000, 10000, 1000, 100, 10, 1];
const STEP_L = ['1M', '100k', '10k', '1k', '100', '10', '1'];
const CSTEP = [1, 10, 100], CSTEP_L = ['0.01', '0.1', '1'];
const OPT = {
  drive: [[2, '2 mA'], [4, '4 mA'], [6, '6 mA'], [8, '8 mA']],
  offState: [['low', 'Low'], ['high', 'High'], ['hiz', 'High-Z']],
  tuning: [['fractional', 'Fractional'], ['lownoise', 'Low noise'], ['integer', 'Integer']],
  spread: [['off', 'Off'], ['down05', '-0.5 %'], ['down15', '-1.5 %'], ['center05', '+/-0.5 %']],
  mode: [['off', 'Off'], ['client', 'Client'], ['ap', 'Access point']]
};
const SSC = {down05: '-0.5 %', down15: '-1.5 %', center05: '+/-0.5 %'};
const STT = {ON: 'ON', OFF: 'OFF', ERR: '! ERR', WAIT: 'WAIT', NOHW: '! NO HW'};
const SAVE = {saved: 'Saved', pending: 'Saving...', failed: 'Save failed', unavailable: 'Not saved'};
const HW = {CONN: 'SI5351 connected', NFO: 'SI5351 not found', BLOC: 'I2C bus blocked (line held low)', UNKN: 'Unknown device at 0x60'};
const KEY = {set: 'password set', open: 'open network', none: 'empty'};
const IPK = ['ip', 'mask', 'gateway', 'dns'];
const IPRE = /^((25[0-5]|2[0-4]\d|1?\d?\d)\.){3}(25[0-5]|2[0-4]\d|1?\d?\d)$/;
const NAMERE = /^[a-z0-9]([a-z0-9-]{0,30}[a-z0-9])?$/;

let S = null, NET = null, first = true, sel = 1, online = false, cstep = 1;
let ws = null, wsLive = false, wsRetry = 2000, polling = false, fails = 0, lastWrite = 0;
let pendEnd = 0, pendAt = 0, pendT = 0, toastT = 0, loginP = null, loginRes = null, loginRej = null;
const Q = {}, busy = {};
const U = {ch: {}, card: {}, cardEl: {}, cfg: {}, wifi: {}};

// ---- DOM helpers
function h(tag, p, ...kids) {
  const el = D.createElement(tag);
  for (const k in p || {}) {
    const v = p[k];
    if (v == null || v === false) continue;
    if (k === 'class') el.className = v;
    else if (k === 'text') el.textContent = v;
    else if (k.startsWith('on')) el.addEventListener(k.slice(2), v);
    else el.setAttribute(k, v === true ? '' : v);
  }
  el.append(...kids.flat(9).filter((c) => c != null && c !== false));
  return el;
}
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
function flash(el) {
  if (first) return;
  el.classList.remove('chg');
  void el.offsetWidth;
  el.classList.add('chg');
}
function set(el, t) {
  t = String(t);
  if (el.textContent !== t) { el.textContent = t; flash(el); }
}
function line(el, t, tone) { set(el, t); el.dataset.t = tone || ''; }
function badge(el, st) { set(el, STT[st] || st); el.dataset.s = st; }
function press(b, on) { b.setAttribute('aria-pressed', on ? 'true' : 'false'); }
function setSw(b, on) {
  const v = on ? 'true' : 'false';
  if (b.getAttribute('aria-checked') !== v) { b.setAttribute('aria-checked', v); flash(b); }
}
function selVal(el, key, v) {
  if (busy[key] || el.value === String(v)) return;
  el.value = String(v);
  flash(el);
}
function fill(el, v) { if (D.activeElement !== el) el.value = v; }
function toast(m, ok) {
  const el = $('msg');
  el.textContent = m;
  el.dataset.t = ok ? 'ok' : 'err';
  el.hidden = false;
  clearTimeout(toastT);
  toastT = setTimeout(() => { el.hidden = true; }, 5000);
}
const fail = (e) => toast(e.msg || String(e));

// ---- Formatting (as on the device display)
const pad3 = (n) => String(n).padStart(3, '0');
function fmtF(hz) {
  hz = Math.round(hz);
  if (hz < 1e6) return Math.floor(hz / 1e3) + '.' + pad3(hz % 1e3) + ' kHz';
  const r = hz % 1e6;
  return Math.floor(hz / 1e6) + '.' + pad3(Math.floor(r / 1e3)) + ' ' + pad3(r % 1e3) + ' MHz';
}
function iqLim(hz, lower, dec, trim) {
  const u = 10 ** (6 - dec), r = (lower ? Math.ceil(hz / u) : Math.floor(hz / u)) * u;
  return trim && r % 1e6 === 0 ? String(r / 1e6) : (r / 1e6).toFixed(dec);
}
const fmtPpm = (c) => (c < 0 ? '-' : '+') + (Math.abs(c) / 100).toFixed(2);
const devOf = (a, f) => Math.round((a - f) * 10) / 10 || 0;
function outLine(a, f, ssc) {
  if (!(a > 0)) return ['Out -', false];
  if (ssc) return ['Out ' + fmtF(a) + ' mean, SSC ' + ssc, false];
  const d = devOf(a, f);
  return ['Out ' + fmtF(a) + ' (' + (d < 0 ? '' : '+') + d.toFixed(1) + ' Hz)', Math.abs(d) >= 1];
}
const rangeNote = (f) => (f > 1e8 ? '>100MHz: low level' : f < 1e6 ? '<1MHz: harmonics' : '');
const clampF = (v) => Math.min(FMAX, Math.max(FMIN, v));
const snap = (s) => STEPS.find((x) => x <= s) || 1;

// ---- State access
const chOf = (n) => S.ch.find((c) => c.n === n) || S.ch[n - 1];
const pend = (k, v) => (Q[k] && Q[k].t != null ? Q[k].t : v);
const step = () => pend('step', S.config.step);
const spreadOf = (n) => ((n === 1 || (n === 3 && S.ch3.master === 1)) && SSC[S.config.spread]) || '';
const master = (n) => (n === 3 ? S.ch3.master : n);

// ---- REST
function apiErr(status, j) {
  const raw = j && j.error ? String(j.error) : '';
  const m = /^(-\d+),(.*)$/.exec(raw);
  const msg = (m ? m[2] : raw) || {0: 'No connection to the device', 401: 'Login required',
    403: 'Forbidden: reload the page', 413: 'Request too large', 415: 'Unsupported request',
    429: 'Too many commands', 503: 'Device busy (low memory)'}[status] || 'Error ' + status;
  return {status, code: m ? +m[1] : 0, msg, data: j};
}
async function api(path, body, retried) {
  const post = body !== undefined;
  if (post) {
    if (!online) throw apiErr(0);
    // The device takes at most 20 writes/s per source (W-S11).
    const t = Math.max(Date.now(), lastWrite + 60);
    lastWrite = t;
    if (t > Date.now()) await sleep(t - Date.now());
  }
  const o = {cache: 'no-store', credentials: 'same-origin'};
  if (window.AbortSignal && AbortSignal.timeout) o.signal = AbortSignal.timeout(post ? 8000 : 4000);
  if (post) {
    o.method = 'POST';
    o.headers = {'Content-Type': 'application/json', 'X-CYD-Token': TOKEN};
    o.body = JSON.stringify(body);
  }
  let r, j = null;
  try { r = await fetch(path, o); } catch (e) { throw apiErr(0); }
  if (r.status === 401 && !retried && path !== '/api/login') {
    await login();
    return api(path, body, true);
  }
  try { j = await r.json(); } catch (e) { j = null; }
  if (!r.ok) throw apiErr(r.status, j);
  if (j && j.ch && j.config) apply(j);
  return j;
}
const act = (p) => p.catch(fail);

// Coalesced writes: steps typed or held faster than the device answers are merged.
function queue(k, cur, fn, send) {
  if (!S || !online) return;
  const q = Q[k] || (Q[k] = {t: null, busy: false});
  const base = q.t != null ? q.t : cur;
  const t = fn(base);
  if (t == null || t === base) return;
  q.t = t;
  render();
  pump(q, send);
}
async function pump(q, send) {
  if (q.busy || q.t == null) return;
  q.busy = true;
  const t = q.t;
  try { await send(t); } catch (e) { q.t = null; fail(e); }
  q.busy = false;
  if (q.t === t) q.t = null;
  if (q.t != null) pump(q, send); else render();
}

// ---- Actions
function tune(n, fn) {
  if (!S) return;
  const m = master(n);
  queue('f' + m, chOf(m).frequency, fn, (v) => api('/api/channel/' + m, {frequency: v}));
}
const stepF = (n, k) => tune(n, (b) => clampF(b + k * step()));
function decade(up) {
  tune(sel, (b) => {
    const v = up ? b * 10 : Math.round(b / 10);
    if (v < FMIN || v > FMAX) { toast('8 kHz - 160 MHz'); return null; }
    return v;
  });
}
const setStep = (s) => S && queue('step', S.config.step, () => s, (v) => api('/api/config', {step: v}));
function moveStep(d) {
  if (!S) return;
  queue('step', S.config.step, (b) => STEPS[Math.max(0, Math.min(6, STEPS.indexOf(snap(b)) + d))],
    (v) => api('/api/config', {step: v}));
}
const setEnabled = (n, on) => act(api(n === 3 ? '/api/ch3' : '/api/channel/' + n, {enabled: on}));
const toggle = (n) => S && setEnabled(n, !(n === 3 ? S.ch3.enabled : chOf(n).enabled));
const post3 = (b) => act(api('/api/ch3', b));
const tuneC = (k) => S && queue('corr', S.config.correction,
  (b) => Math.max(-CMAX, Math.min(CMAX, b + k * CSTEP[cstep])), (v) => api('/api/config', {correction: v}));
const tuneBl = (k) => S && queue('bl', S.config.backlight,
  (b) => Math.max(10, Math.min(100, b + k * 10)), (v) => api('/api/config', {backlight: v}));

async function applyText(n) {
  const w = U.ch[n], v = w.in.value.trim();
  if (!S) return;
  if (!v) { line(w.err, 'Enter a number', 'err'); return; }
  try {
    await api('/api/channel/' + master(n), {frequencyText: v});
    discard(n);
    w.in.blur();
  } catch (e) {
    line(w.err, e.code === -222 ? e.msg + ': 8 kHz - 160 MHz' : e.msg, 'err');
    w.in.classList.add('bad');
  }
}
function discard(n) {
  const w = U.ch[n];
  w.in.value = '';
  w.in.classList.remove('bad');
  w.err.textContent = '';
}
async function applyCorr() {
  const C = U.cfg, s = C.corr.value.replace(/ppm/i, '').replace(',', '.').replace(/\s/g, '');
  const ok = /^[+-]?(\d+\.?\d*|\.\d+)$/.test(s), v = Math.round(parseFloat(s) * 100);
  if (!ok || Math.abs(v) > CMAX) { line(C.cerr, '-50.00 to +50.00 ppm', 'err'); C.corr.classList.add('bad'); return; }
  try {
    await api('/api/config', {correction: v});
    C.cerr.textContent = '';
    C.corr.classList.remove('bad');
    C.corr.blur();
  } catch (e) { line(C.cerr, e.msg, 'err'); }
}
async function cfgSel(k) {
  const el = U.cfg[k];
  const v = k === 'drive' ? +el.value : el.value;
  if (k === 'offState' && v === 'high' && S.config.offState !== 'high' && !(await ask('Off state High',
    'Switched-off outputs then carry DC (supply voltage, mostly 3.3 V). Do not use with DC-coupled mixers or inputs.', 'High'))) {
    el.value = S.config.offState;
    return;
  }
  busy[k] = 1;
  try { await api('/api/config', {[k]: v}); } catch (e) { fail(e); }
  busy[k] = 0;
  render();
}

// ---- Dialogs
function ask(title, text, ok) {
  return new Promise((res) => {
    const d = $('dlgAsk');
    $('askT').textContent = title;
    $('askX').textContent = text;
    $('askOk').textContent = ok;
    d.returnValue = '';
    d.onclose = () => res(d.returnValue === 'ok');
    d.showModal();
  });
}
function login() {
  if (!loginP) {
    loginP = new Promise((res, rej) => { loginRes = res; loginRej = rej; });
    $('loginErr').textContent = '';
    $('dlgLogin').showModal();
  }
  return loginP;
}
async function doLogin(e) {
  e.preventDefault();
  const pw = $('pw'), err = $('loginErr');
  try {
    const r = await fetch('/api/login', {method: 'POST', credentials: 'same-origin', cache: 'no-store',
      headers: {'Content-Type': 'application/json', 'X-CYD-Token': TOKEN}, body: JSON.stringify({password: pw.value})});
    pw.value = '';
    if (r.ok) {
      const res = loginRes;
      loginP = null;
      $('dlgLogin').close();
      res();
      wsRetry = 2000;
      connectWs();
      return;
    }
    let j = null;
    try { j = await r.json(); } catch (x) { j = null; }
    err.textContent = r.status === 401 ? 'Wrong password'
      : r.status === 429 ? 'Locked: try again in ' + ((j && j.retryAfter) || 30) + ' s' : apiErr(r.status, j).msg;
  } catch (x) { err.textContent = 'No connection to the device'; }
}

// ---- Connection: WebSocket push, polling as fallback
function setOnline(v) {
  online = v;
  $('ui').disabled = !v;
  $('bIdent').disabled = $('bSave').disabled = !v;
  D.body.classList.toggle('offline', !v);
  const c = $('conn');
  c.textContent = wsLive ? 'Live' : v ? 'Polling' : 'Offline';
  c.dataset.t = wsLive ? 'ok' : v ? '' : 'err';
}
function apply(st) {
  if (!st || !Array.isArray(st.ch) || !st.ch3 || !st.config) return;
  S = st;
  render();
  first = false;
}
async function poll() {
  if (polling) return;
  polling = true;
  while (!wsLive) {
    try {
      apply(await api('/api/state'));
      fails = 0;
      setOnline(true);
    } catch (e) {
      if (++fails >= 2) setOnline(false);
    }
    await sleep(1000);
  }
  polling = false;
}
function connectWs() {
  if (ws || !window.WebSocket) return;
  try {
    ws = new WebSocket((location.protocol === 'https:' ? 'wss://' : 'ws://') + location.host + '/ws?t=' + encodeURIComponent(TOKEN));
  } catch (e) { ws = null; return; }
  ws.onopen = () => { wsLive = true; wsRetry = 2000; fails = 0; setOnline(true); };
  ws.onmessage = (e) => { try { apply(JSON.parse(e.data)); } catch (x) { /* not a state */ } };
  ws.onclose = () => {
    ws = null;
    wsLive = false;
    setOnline(online);
    poll();
    setTimeout(connectWs, wsRetry);
    wsRetry = Math.min(wsRetry * 2, 30000);
  };
}
// Push only on change: a slow GET confirms the link and catches a missed message.
async function heartbeat() {
  if (!wsLive) return;
  try { apply(await api('/api/state')); } catch (e) { if (e.status !== 401 && ws) ws.close(); }
}

// ---- Render
function modeLine(el, modes, ...extra) {
  line(el, modes.concat(extra).filter(Boolean).join(' | '), modes.includes('OFF=High') ? 'warn' : '');
}
function showDigits(el, hz, st) {
  const k = hz + '/' + st;
  if (el.dataset.k === k) return;
  const chg = el.dataset.k && el.dataset.k.split('/')[0] !== String(hz);
  el.dataset.k = k;
  const t = fmtF(hz), se = Math.round(Math.log10(st));
  let left = t.replace(/\D/g, '').length;
  const kids = [];
  for (const c of t.slice(0, -4)) {
    if (c >= '0' && c <= '9') {
      left--;
      kids.push(h('span', {class: left === se ? 'dg cur' : 'dg', 'data-e': left, text: c}));
    } else kids.push(c === ' ' ? h('span', {class: 'sp'}) : c);
  }
  kids.push(h('span', {class: 'un', text: t.slice(-3)}));
  el.replaceChildren(...kids);
  if (chg) flash(el);
}
const approx = (a, f) => S.config.tuning === 'integer' && S.hw === 'CONN' && a > 0 && f < 1e8 && Math.abs(devOf(a, f)) >= 1;

function render() {
  if (!S) return;
  const cf = S.config, c3 = S.ch3, wf = S.wifi || {}, st = step();
  set($('dev'), [wf.name, S.device && S.device.version && 'v' + S.device.version].filter(Boolean).join(' · '));
  line($('save'), SAVE[S.save] || S.save, S.save === 'failed' || S.save === 'unavailable' ? 'err' : '');
  const rem = $('remote'), hw = $('hw');
  rem.hidden = !S.remote;
  if (S.remote) set(rem, 'REMOTE ' + String(S.remote).toUpperCase());
  hw.hidden = S.hw === 'CONN';
  set(hw, HW[S.hw] || S.hw);

  for (const n of [1, 2]) {
    const c = chOf(n), f = pend('f' + n, c.frequency), card = U.card[n], w = U.ch[n], modes = c.modes || [];
    const iq = c3.master === n && c3.status !== 'OFF';
    badge(card.st, c.status);
    badge(w.st, c.status);
    setSw(card.sw, c.enabled);
    setSw(w.sw, c.enabled);
    set(card.f, (approx(c.actual, c.frequency) && f === c.frequency ? '~' : '') + fmtF(f));
    modeLine(card.d, modes, iq ? (modes.length ? 'IQ master' : 'IQ master of CH3') : '');
    showDigits(w.f, f, st);
    if (c.status === 'NOHW' || c.status === 'ERR') {
      line(w.out, c.status === 'NOHW' ? 'SI5351 not found' : 'Output error, retrying', 'err');
    } else {
      const o = outLine(c.actual, c.frequency, spreadOf(n));
      line(w.out, (c.status === 'OFF' ? 'Off | ' : '') + o[0], o[1] ? 'warn' : '');
    }
    modeLine(w.m, modes, iq ? 'IQ master (' + c3.phase + '°)' : '', rangeNote(f));
    w.steps.forEach((b, i) => press(b, STEPS[i] === snap(st)));
  }

  const m = c3.master, mc = chOf(m), w = U.ch[3], card = U.card[3], f3 = pend('f' + m, c3.frequency);
  const lo = c3.iqMin, hi = c3.iqMax;
  let reason = '', info = '', tone = '', wake = false;
  if (c3.status === 'WAIT') {
    tone = 'warn';
    if (!mc.enabled) {
      reason = ' / master off';
      info = 'Master CH' + m + ' is off: click to switch on';
      wake = true;
    } else if (mc.frequency < lo) {
      reason = ' / < ' + iqLim(lo, 1, 2) + ' MHz';
      info = 'Waiting: CH' + m + ' below ' + iqLim(lo, 1, 3) + ' MHz';
    } else {
      reason = ' / > ' + iqLim(hi, 0, 2) + ' MHz';
      info = 'Waiting: CH' + m + ' above ' + iqLim(hi, 0, 3) + ' MHz';
    }
  } else if (c3.status === 'ERR') {
    reason = ' / setup failed';
    info = 'IQ setup failed, retrying';
    tone = 'err';
  } else if (c3.status === 'NOHW') {
    info = 'SI5351 not found';
    tone = 'err';
  } else if (c3.status === 'OFF') {
    info = 'Off  |  follows CH' + m;
  } else {
    const o = outLine(c3.actual, c3.frequency, spreadOf(3));
    info = o[0];
    tone = o[1] ? 'warn' : '';
  }
  badge(card.st, c3.status);
  badge(w.st, c3.status);
  setSw(card.sw, c3.enabled);
  setSw(w.sw, c3.enabled);
  set(card.f, (approx(c3.actual, c3.frequency) ? '~' : '') + fmtF(f3));
  line(card.d, 'CH' + m + ' / ' + c3.phase + '°' + reason, '');
  showDigits(w.f, f3, st);
  w.wake.hidden = !wake;
  w.out.hidden = wake;
  if (wake) set(w.wake, info); else line(w.out, info, tone);
  modeLine(w.m, mc.modes || []);
  w.ms.forEach((b, i) => press(b, m === i + 1));
  w.ph.forEach((b, i) => press(b, c3.phase === i * 90));
  set(w.note, 'IQ range ' + iqLim(lo, 1, 3) + ' - ' + iqLim(hi, 0, 3, 1) +
    ' MHz. 180°/270° use output inversion. Drive strength is set globally in CONFIG.');

  const C = U.cfg, lock = cf.tuning === 'integer';
  selVal(C.drive, 'drive', cf.drive);
  selVal(C.offState, 'offState', cf.offState);
  selVal(C.tuning, 'tuning', cf.tuning);
  C.spread.disabled = lock;
  C.spread.options[0].textContent = lock ? 'Off (Integer)' : 'Off';
  selVal(C.spread, 'spread', cf.spread);
  const pv = fmtPpm(pend('corr', cf.correction));
  if (D.activeElement !== C.corr && C.corr.value !== pv) { C.corr.value = pv; flash(C.corr); }
  set(C.bl, pend('bl', cf.backlight) + ' %');
  const local = cf.remoteOutputs === 'local';
  line(C.remote, local ? 'local only — switch on at the device' : 'allowed', local ? 'warn' : '');
  U.local.hidden = !local;
  set(C.fw, (S.device && S.device.version) || '-');
  line(C.hw, HW[S.hw] || S.hw, S.hw === 'CONN' ? '' : 'err');
  set(C.name, wf.name || '-');
  renderWifi(wf);
}

function renderWifi(wf) {
  const W = U.wifi, ap = wf.ap || {};
  set(W.sMode, (OPT.mode.find((o) => o[0] === wf.mode) || [0, wf.mode || '-'])[1]);
  set(W.sNet, wf.mode === 'client' ? (wf.connected ? 'Connected to ' + wf.ssid + ' (' + wf.rssi + ' dBm)' : 'Not connected') : '-');
  set(W.sIp, wf.ip || '-');
  line(W.sAp, ap.active ? ap.ssid + (ap.open ? ' (open)' : '') + ', ' + ap.stations + (ap.stations === 1 ? ' station' : ' stations') : 'off',
    ap.active && ap.open ? 'warn' : '');
  set(W.sAddr, wf.name ? 'http://' + wf.name + '.local/' : '-');
  W.openWarn.hidden = !(ap.active && ap.open);
  // Optional (proposed) field: a pending change is also visible over the new connection.
  if (typeof wf.confirmWithin === 'number') {
    if (wf.confirmWithin > 0 && !pendEnd) startPending(wf.confirmWithin, '');
    else if (wf.confirmWithin <= 0 && pendEnd && Date.now() - pendAt > 3000) stopPending();
  }
}

// ---- Building the page
function sw(n) {
  return h('button', {type: 'button', class: 'sw', role: 'switch', 'aria-checked': 'false',
    'aria-label': 'CH' + n + ' output', onclick: () => toggle(n)});
}
// Holding repeats as on the device: after 0.6 s about 3 steps/s, from 3 s 10/s, from 6 s 20/s.
function rep(text, fn, label, cls) {
  let tm = 0, t0 = 0;
  const stop = () => { clearTimeout(tm); tm = 0; };
  const tick = () => {
    if (!online) return stop();
    const el = Date.now() - t0;
    fn(el >= 6000 ? 2 : 1);
    tm = setTimeout(tick, el >= 3000 ? 100 : 300);
  };
  return h('button', {type: 'button', class: cls || 'big', 'aria-label': label, text,
    onpointerdown: (e) => {
      if (e.button) return;
      e.preventDefault();
      stop();
      t0 = Date.now();
      fn(1);
      tm = setTimeout(tick, 600);
    },
    onpointerup: stop, onpointerleave: stop, onpointercancel: stop,
    onclick: (e) => { if (e.detail === 0) fn(1); },
    oncontextmenu: (e) => e.preventDefault()});
}
function digits(n) {
  const el = h('div', {class: 'freq', title: 'Click a digit: use it as the step. Mouse wheel over a digit: change it.'});
  let acc = 0, last = 0;
  el.addEventListener('click', (e) => {
    const d = e.target.closest('.dg');
    if (d) setStep(10 ** Math.min(6, +d.dataset.e));
    else if (e.target.closest('.un')) U.ch[n].in.focus();
  });
  el.addEventListener('wheel', (e) => {
    const d = e.target.closest('.dg');
    if (!d || !online || !S) return;
    e.preventDefault();
    const now = Date.now();
    if (now - last > 400) acc = 0;
    last = now;
    acc += e.deltaMode ? e.deltaY * 50 : e.deltaY;
    if (Math.abs(acc) < 50) return;
    const dir = acc < 0 ? 1 : -1;
    acc = 0;
    tune(n, (b) => clampF(b + dir * 10 ** +d.dataset.e));
  }, {passive: false});
  return el;
}
function freqForm(n, w) {
  return h('form', {class: 'fin', autocomplete: 'off', onsubmit: (e) => { e.preventDefault(); applyText(n); }},
    h('label', {for: 'f' + n, text: n === 3 ? 'Master frequency' : 'Frequency'}),
    h('div', {class: 'row'},
      w.in = h('input', {id: 'f' + n, type: 'text', class: 'fi', 'data-ch': n, maxlength: 32, spellcheck: 'false',
        autocapitalize: 'off', enterkeyhint: 'done', placeholder: '10.7M, 455k, 7 100 000'}),
      h('button', {class: 'pri', text: 'Set'})),
    w.err = h('div', {class: 'err', 'aria-live': 'polite'}));
}
function buildMain() {
  const card = (n) => {
    const c = U.card[n] = {};
    return U.cardEl[n] = h('div', {class: 'card c' + n, onclick: (e) => { if (!e.target.closest('.sw')) select(n, true); }},
      h('div', {class: 'cl'},
        h('div', {class: 'chd'}, h('b', {text: 'CH' + n}), c.st = h('span', {class: 'st'})),
        c.f = h('div', {class: 'cf'}),
        c.d = h('div', {class: 'cd'})),
      c.sw = sw(n));
  };
  return h('section', {class: 'outs', id: 'main', 'aria-label': 'MAIN'},
    h('h2', {text: 'MAIN'}),
    h('div', {class: 'cards'}, [1, 2, 3].map(card)),
    U.local = h('p', {class: 'note', hidden: true, text: 'Remote outputs: local only — switch on at the device'}));
}
function buildCh(n) {
  const w = U.ch[n] = {};
  return h('section', {class: 'area c' + n, id: 'ch' + n, onpointerdown: () => select(n), onfocusin: () => select(n)},
    h('div', {class: 'ah'}, h('h2', {text: n === 3 ? 'CH3 IQ' : 'CH' + n}), w.sw = sw(n), w.st = h('span', {class: 'st'})),
    w.f = digits(n),
    w.out = h('div', {class: 'out'}),
    n === 3 && (w.wake = h('button', {type: 'button', class: 'link', hidden: true, onclick: () => setEnabled(S.ch3.master, true)})),
    w.m = h('div', {class: 'modes'}),
    n === 3 ? [
      h('div', {class: 'iq'},
        h('div', null, h('div', {class: 'lbl', text: 'Master'}),
          h('div', {class: 'chips'}, w.ms = [1, 2].map((k) => h('button', {type: 'button', class: 'chip c' + k, text: 'CH' + k, onclick: () => post3({master: k})})))),
        h('div', null, h('div', {class: 'lbl', text: 'Phase (°)'}),
          h('div', {class: 'chips'}, w.ph = [0, 90, 180, 270].map((p) => h('button', {type: 'button', class: 'chip', text: String(p), onclick: () => post3({phase: p})}))))),
      w.note = h('p', {class: 'mu sm'})
    ] : [
      h('div', {class: 'chips steps', role: 'group', 'aria-label': 'Step'},
        w.steps = STEPS.map((s, i) => h('button', {type: 'button', class: 'chip', text: STEP_L[i], onclick: () => setStep(s)}))),
      h('div', {class: 'pm'}, rep('−', (k) => stepF(n, -k), 'Frequency down'), rep('+', (k) => stepF(n, k), 'Frequency up'))
    ],
    freqForm(n, w));
}
function row(label, ctl, id) {
  return h('div', {class: 'crow'}, h('label', {class: 'lbl', for: id, text: label}), ctl);
}
function buildCfg() {
  const C = U.cfg;
  const dd = (k, label) => row(label, C[k] = h('select', {id: 'k-' + k, onchange: () => cfgSel(k)},
    OPT[k].map((o) => h('option', {value: o[0], text: o[1]}))), 'k-' + k);
  return h('section', {class: 'area cfg', id: 'config'},
    h('h2', {text: 'CONFIG'}),
    dd('drive', 'Drive all'), dd('offState', 'Off state'), dd('tuning', 'Tuning'), dd('spread', 'Spread CH1'),
    row('Corr.', h('div', null,
      h('form', {class: 'ctl', onsubmit: (e) => { e.preventDefault(); applyCorr(); }},
        C.corr = h('input', {id: 'k-corr', type: 'text', class: 'ppm', inputmode: 'decimal', maxlength: 10, spellcheck: 'false',
          title: 'Correction in ppm, Enter applies, Esc discards'}),
        h('span', {class: 'mu', text: 'ppm'}),
        C.cstep = h('button', {type: 'button', class: 'chip', text: CSTEP_L[cstep], title: 'Step in ppm',
          onclick: () => { cstep = (cstep + 1) % 3; C.cstep.textContent = CSTEP_L[cstep]; }}),
        rep('−', (k) => tuneC(-k), 'Correction down', 'big sm'), rep('+', (k) => tuneC(k), 'Correction up', 'big sm')),
      C.cerr = h('div', {class: 'err'})), 'k-corr'),
    row('Backlight', h('div', {class: 'ctl'},
      h('button', {type: 'button', class: 'big sm', text: '−', 'aria-label': 'Backlight down', onclick: () => tuneBl(-1)}),
      C.bl = h('span', {class: 'val'}),
      h('button', {type: 'button', class: 'big sm', text: '+', 'aria-label': 'Backlight up', onclick: () => tuneBl(1)}))),
    row('Remote outputs', C.remote = h('span', {class: 'val'})),
    h('dl', {class: 'info'},
      h('dt', {text: 'Firmware'}), C.fw = h('dd'),
      h('dt', {text: 'SI5351'}), C.hw = h('dd'),
      h('dt', {text: 'Device'}), C.name = h('dd')),
    h('p', {class: 'mu sm', text: 'Touch calibration, screen lock, reset and Remote outputs are set at the device.'}));
}

// ---- WiFi
function buildWifi() {
  const W = U.wifi;
  const fld = (label, el) => h('label', {class: 'fld'}, h('span', {text: label}), el);
  const inp = (p) => h('input', Object.assign({type: 'text', autocomplete: 'off', spellcheck: 'false', autocapitalize: 'off'}, p));
  W.slots = [0, 1].map(() => ({}));
  return h('section', {class: 'area cfg wifi', id: 'wifi'},
    h('h2', {text: 'WiFi'}),
    h('dl', {class: 'info'},
      h('dt', {text: 'Mode'}), W.sMode = h('dd'),
      h('dt', {text: 'Network'}), W.sNet = h('dd'),
      h('dt', {text: 'IP'}), W.sIp = h('dd'),
      h('dt', {text: 'Access point'}), W.sAp = h('dd'),
      h('dt', {text: 'Address'}), W.sAddr = h('dd')),
    W.openWarn = h('p', {class: 'note', hidden: true, text: 'Access point open: passwords cannot be sent over it.'}),
    W.pend = h('div', {class: 'pend', hidden: true},
      W.pendT = h('p', {class: 'big-t'}),
      W.addr = h('p'),
      h('p', {class: 'mu sm', text: 'Without a web password, confirm at the device as well.'}),
      h('button', {type: 'button', class: 'pri', text: 'Confirm', onclick: confirmNet})),
    h('form', {class: 'wf', autocomplete: 'off', onsubmit: applyNet},
      h('div', {class: 'grid2'},
        fld('Mode', W.mode = h('select', null, OPT.mode.map((o) => h('option', {value: o[0], text: o[1]})))),
        fld('Device name', W.name = inp({maxlength: 32, title: 'a-z, 0-9 and -, max. 32'}))),
      h('h3', {text: 'Networks'}),
      W.slots.map((s, i) => h('div', {class: 'grid2 slot'},
        fld('Network ' + (i + 1), s.ssid = inp({maxlength: 32, placeholder: 'SSID'})),
        fld('Password', s.pw = h('input', {type: 'password', autocomplete: 'new-password', maxlength: 63})),
        s.key = h('span', {class: 'mu sm'}),
        h('label', {class: 'chk'}, s.del = h('input', {type: 'checkbox'}), 'Delete'))),
      h('div', {class: 'row'}, W.scanB = h('button', {type: 'button', text: 'Scan', onclick: scan}), W.scanS = h('span', {class: 'mu'})),
      W.scanL = h('ul', {class: 'scan'}),
      h('h3', {text: 'Static IP'}),
      h('label', {class: 'chk'}, W.sip = h('input', {type: 'checkbox', onchange: syncSip}), 'Use a static IP (otherwise DHCP)'),
      h('div', {class: 'grid2'}, IPK.map((k) => fld({ip: 'IP', mask: 'Mask', gateway: 'Gateway', dns: 'DNS'}[k],
        W[k] = inp({inputmode: 'decimal', maxlength: 15, placeholder: k === 'mask' ? '255.255.255.0' : ''})))),
      h('h3', {text: 'Access point'}),
      h('dl', {class: 'info'}, h('dt', {text: 'Name'}), W.apSsid = h('dd'), h('dt', {text: 'Password'}), W.apKey = h('dd')),
      h('label', {class: 'chk'}, W.apOpen = h('input', {type: 'checkbox'}), 'Open (not recommended)'),
      fld('New AP password', W.apPw = h('input', {type: 'password', autocomplete: 'new-password', minlength: 8, maxlength: 63, placeholder: 'unchanged'})),
      W.err = h('p', {class: 'err', 'aria-live': 'polite'}),
      h('div', {class: 'row'}, h('button', {class: 'pri', text: 'Apply network settings'}),
        h('button', {type: 'button', text: 'Reload', onclick: () => loadNet(true)}))));
}
function syncSip() { IPK.forEach((k) => { U.wifi[k].disabled = !U.wifi.sip.checked; }); }
function fillNet(n) {
  const W = U.wifi;
  NET = n;
  fill(W.mode, n.mode);
  fill(W.name, n.name || '');
  W.slots.forEach((s, i) => {
    const x = (n.networks || [])[i] || {ssid: '', key: 'none'};
    fill(s.ssid, x.ssid || '');
    s.pw.value = '';
    s.pw.placeholder = x.key === 'set' ? 'unchanged' : 'none (open network)';
    s.key.textContent = KEY[x.key] || '';
    s.del.checked = false;
    s.del.disabled = x.key === 'none';
  });
  const si = n.staticIp || {};
  W.sip.checked = !!si.enabled;
  IPK.forEach((k) => fill(W[k], si[k] || ''));
  syncSip();
  const ap = n.ap || {};
  W.apSsid.textContent = ap.ssid || '-';
  W.apKey.textContent = ap.passwordSet ? 'set' : 'none';
  W.apOpen.checked = !!ap.open;
  W.apPw.value = '';
}
async function loadNet(show) {
  try {
    fillNet(await api('/api/wifi'));
    U.wifi.err.textContent = '';
  } catch (e) { if (show || e.status) line(U.wifi.err, 'WiFi settings: ' + e.msg, 'err'); }
}
// The request holds only what changed; passwords only when typed.
function netBody() {
  const W = U.wifi, n = NET, b = {}, nets = [], del = [];
  if (W.mode.value !== n.mode) b.mode = W.mode.value;
  if (W.name.value !== (n.name || '')) {
    if (!NAMERE.test(W.name.value)) return 'Device name: a-z, 0-9 and -, max. 32 characters';
    b.name = W.name.value;
  }
  for (let i = 0; i < 2; i++) {
    const s = W.slots[i], cur = (n.networks || [])[i] || {ssid: '', key: 'none'}, ssid = s.ssid.value, pw = s.pw.value;
    if (s.del.checked || (!ssid && cur.ssid)) { if (cur.key !== 'none') del.push(i); continue; }
    if (pw && (pw.length < 8 || pw.length > 63)) return 'Network ' + (i + 1) + ': password with 8 to 63 characters';
    if (ssid && (ssid !== cur.ssid || pw)) {
      const e = {slot: i, ssid};
      if (pw || ssid !== cur.ssid) e.password = pw;
      nets.push(e);
    }
  }
  if (nets.length) b.networks = nets;
  if (del.length) b.delete = del;
  const si = n.staticIp || {}, ns = {enabled: W.sip.checked};
  IPK.forEach((k) => { ns[k] = W[k].value.trim(); });
  if (IPK.some((k) => ns[k] !== (si[k] || '')) || ns.enabled !== !!si.enabled) {
    if (ns.enabled && !IPK.every((k) => IPRE.test(ns[k]) || (k === 'dns' && !ns[k]))) return 'Static IP: enter IP, mask and gateway as a.b.c.d';
    b.staticIp = ns;
  }
  const ap = {};
  if (W.apOpen.checked !== !!(n.ap && n.ap.open)) ap.open = W.apOpen.checked;
  if (W.apPw.value) {
    if (W.apPw.value.length < 8) return 'AP password: 8 to 63 characters';
    ap.password = W.apPw.value;
  }
  if (Object.keys(ap).length) b.ap = ap;
  return b;
}
async function applyNet(e) {
  e.preventDefault();
  const W = U.wifi;
  if (!NET) return loadNet(true);
  const b = netBody();
  if (typeof b === 'string') return line(W.err, b, 'err');
  if (!Object.keys(b).length) return line(W.err, 'No changes', '');
  const open = (b.networks || []).find((x) => x.password === '');
  if (open && !(await ask('Open network', 'Network ' + (open.slot + 1) + ' is saved without a password. Its traffic is not encrypted.', 'Save'))) return;
  if (b.ap && b.ap.open && !(await ask('Open access point', 'Anyone nearby can join it and control the generator. Passwords cannot be changed over an open access point.', 'Open'))) return;
  if (b.mode === 'off' && !(await ask('Switch WiFi off', 'The web interface is then no longer reachable; switch WiFi on again at the device.', 'Switch off'))) return;
  try {
    const r = await api('/api/wifi', b);
    W.slots.forEach((s) => { s.pw.value = ''; });
    W.apPw.value = '';
    W.err.textContent = '';
    startPending((r && r.confirmWithin) || 120, newAddr(b));
  } catch (x) { line(W.err, x.msg, 'err'); }
}
function newAddr(b) {
  const name = b.name || NET.name, mode = b.mode || NET.mode, ip = b.staticIp && b.staticIp.enabled && b.staticIp.ip;
  if (mode === 'off') return 'WiFi switches off.';
  return (mode === 'ap' && NET.mode !== 'ap' ? 'Join the WiFi ' + ((NET.ap && NET.ap.ssid) || '') + ' first. ' : '') +
    'If this page loses the connection, open http://' + name + '.local/' + (ip ? ' or http://' + ip + '/' : '') + ' and confirm there.';
}
function startPending(sec, addr) {
  pendEnd = Date.now() + sec * 1000;
  pendAt = Date.now();
  U.wifi.addr.textContent = addr;
  U.wifi.pend.hidden = false;
  clearTimeout(pendT);
  tickPending();
}
function stopPending() {
  pendEnd = 0;
  clearTimeout(pendT);
  U.wifi.pend.hidden = true;
}
function tickPending() {
  const s = Math.ceil((pendEnd - Date.now()) / 1000);
  if (s <= 0) {
    stopPending();
    line(U.wifi.err, 'Not confirmed: the previous settings return.', 'err');
    setTimeout(() => loadNet(), 3000);
    return;
  }
  U.wifi.pendT.textContent = 'Confirm within ' + s + ' s, otherwise the previous settings return.';
  pendT = setTimeout(tickPending, 1000);
}
async function confirmNet() {
  try {
    await api('/api/wifi/confirm', {});
    stopPending();
    toast('Network settings confirmed', true);
    loadNet();
  } catch (e) { line(U.wifi.err, e.msg, 'err'); }
}
async function scan() {
  const W = U.wifi, t0 = Date.now();
  W.scanB.disabled = true;
  W.scanS.textContent = 'Scanning...';
  try {
    let r = null;
    for (let i = 0; i < 30; i++) {
      r = await api('/api/wifi/scan');
      if (!r || r.state !== 'running') break;
      await sleep(1000);
    }
    if (!r || r.state !== 'done') W.scanS.textContent = 'Scan failed';
    else {
      const list = (r.networks || []).slice(0, 15).sort((a, b) => b.rssi - a.rssi);
      W.scanS.textContent = list.length + (list.length === 1 ? ' network' : ' networks');
      W.scanL.replaceChildren(...list.map((x) => h('li', null,
        h('span', {class: 'ss', text: x.ssid || '(hidden)'}),
        h('span', {class: 'mu sm', text: x.rssi + ' dBm' + (x.secure ? '' : ', open')}),
        [0, 1].map((i) => h('button', {type: 'button', class: 'chip', text: '→ ' + (i + 1), 'aria-label': 'Use for network ' + (i + 1),
          onclick: () => { const s = W.slots[i]; s.ssid.value = x.ssid; s.del.checked = false; s.pw.value = ''; s.pw.focus(); }})))));
    }
  } catch (e) {
    W.scanS.textContent = e.status === 429 ? 'Wait 10 s between scans' : e.msg;
  }
  setTimeout(() => { W.scanB.disabled = false; }, Math.max(0, 10000 - (Date.now() - t0)));
}

// ---- Selection and keyboard (ARBEITSPLAN 3.6)
function select(n, scroll) {
  sel = n;
  for (const k of [1, 2, 3]) {
    $('ch' + k).classList.toggle('sel', k === n);
    U.cardEl[k].classList.toggle('sel', k === n);
  }
  if (scroll) $('ch' + n).scrollIntoView({block: 'nearest', behavior: 'smooth'});
}
function focusIn() {
  const i = U.ch[sel].in;
  i.focus();
  i.select();
}
function onKey(e) {
  if (e.ctrlKey || e.altKey || e.metaKey || e.isComposing || D.querySelector('dialog[open]')) return;
  const t = e.target, k = e.key;
  if (t.matches && t.matches('input, select, textarea')) {
    const n = +t.dataset.ch;
    if (n && k === 'Escape') { discard(n); t.blur(); e.preventDefault(); }
    else if (n && (k === 'ArrowUp' || k === 'ArrowDown')) { stepF(n, k === 'ArrowUp' ? 1 : -1); e.preventDefault(); }
    else if (t === U.cfg.corr && k === 'Escape') { U.cfg.cerr.textContent = ''; t.classList.remove('bad'); t.blur(); render(); }
    return;
  }
  if (k === '?') { $('dlgHelp').showModal(); e.preventDefault(); return; }
  if (!S || !online) return;
  switch (k) {
    case '1': case '2': case '3': select(+k, true); break;
    case 'ArrowUp': case '+': stepF(sel, 1); break;
    case 'ArrowDown': case '-': stepF(sel, -1); break;
    case 'ArrowLeft': moveStep(-1); break;
    case 'ArrowRight': moveStep(1); break;
    case 'PageUp': decade(true); break;
    case 'PageDown': decade(false); break;
    case 'f': case 'F': focusIn(); break;
    case 'Enter': if (t.closest && t.closest('button, a')) return; focusIn(); break;
    case 'o': case 'O': toggle(sel); break;
    default: return;
  }
  e.preventDefault();
}

function init() {
  $('ui').append(buildMain(), h('div', {class: 'grid'}, buildCh(1), buildCh(2), buildCh(3), buildCfg(), buildWifi()));
  $('bIdent').addEventListener('click', () => act(api('/api/identify', {}).then(() => toast('Identify: display and LED blink for 5 s', true))));
  $('bSave').addEventListener('click', () => act(api('/api/save', {})));
  $('bHelp').addEventListener('click', () => $('dlgHelp').showModal());
  $('helpX').addEventListener('click', () => $('dlgHelp').close());
  $('askOk').addEventListener('click', () => $('dlgAsk').close('ok'));
  $('askNo').addEventListener('click', () => $('dlgAsk').close('no'));
  $('fLogin').addEventListener('submit', doLogin);
  $('dlgLogin').addEventListener('close', () => {
    if (loginP) { loginP = null; loginRej(apiErr(401)); }
  });
  D.querySelectorAll('nav a[data-ch]').forEach((a) => a.addEventListener('click', () => select(+a.dataset.ch)));
  D.addEventListener('keydown', onKey);
  select(1);
  poll();
  connectWs();
  setInterval(heartbeat, 15000);
  loadNet();
}
init();
})();
