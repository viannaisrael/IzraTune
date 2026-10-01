import * as Juce from './juce_frontend.js';
const P = document.getElementById('panel'), $ = (h) => { const t = document.createElement('div'); t.innerHTML = h.trim(); return t.firstChild; };
const put = (el, x, y) => { el.style.left = x + 'px'; el.style.top = (y - 82) + 'px'; P.appendChild(el); return el; };
const lab = (t, x, y, c = '') => put($(`<div class="lab ${c}">${t}</div>`), x, y);

/* ---------- JUCE bridge (with a mock so the page previews in a normal browser) ---------- */
const stat = document.getElementById('stat');
const report = m => { stat.textContent += ' | ' + m; };
window.addEventListener('unhandledrejection', e => report('promise: ' + (e.reason && e.reason.message || e.reason)));
let hasJuce = false;
if (window.__JUCE__) {
  if (typeof Juce.getSliderState === 'function') hasJuce = true;
  else report('getSliderState nao encontrado; exports: ' + Object.keys(Juce));
} else report('modo previa (sem JUCE)');
const mockS = (a, b, d) => { let v = (d - a) / (b - a); const l = []; return { properties: { start: a, end: b }, getNormalisedValue: () => v,
  setNormalisedValue: x => { v = x; l.forEach(f => f()); }, sliderDragStarted() {}, sliderDragEnded() {}, valueChangedEvent: { addListener: f => l.push(f) } }; };
const mockT = () => { let v = false; const l = []; return { getValue: () => v, setValue: x => { v = x; l.forEach(f => f()); }, valueChangedEvent: { addListener: f => l.push(f) } }; };
const defs = { pitch: [-12, 12, 0], inGain: [-24, 24, 0], outTrim: [-24, 24, 0], outGain: [-24, 24, 0], tone: [0, 1, 1] };
const S = {}; for (const k in defs) S[k] = hasJuce ? Juce.getSliderState(k) : mockS(...defs[k]);
const PH = hasJuce ? Juce.getToggleState('phase') : mockT();
const rng = k => { const p = S[k].properties; return [p.start ?? defs[k][0], p.end ?? defs[k][1]]; };
const get = k => { const [a, b] = rng(k); return a + S[k].getNormalisedValue() * (b - a); };
const set = (k, v) => { const [a, b] = rng(k); S[k].sliderDragStarted(); S[k].setNormalisedValue(Math.min(1, Math.max(0, (v - a) / (b - a)))); S[k].sliderDragEnded(); };
const fire = [];  const on = f => { fire.push(f); f(); };
for (const k in S) S[k].valueChangedEvent.addListener(() => fire.forEach(f => f()));

/* ---------- knobs ---------- */
const db = v => (Math.abs(v) < 0.05 ? '0dB' : v.toFixed(1) + 'dB');
const arcPt = (a, r) => [48 + r * Math.sin(a * Math.PI / 180), 48 - r * Math.cos(a * Math.PI / 180)];
function knob(key, x, y, fmt) {
  const el = put($(`<div class="knob abs"><svg viewBox="0 0 96 96">
    <defs><radialGradient id="g${key}" cx="50%" cy="30%" r="80%"><stop offset="0" stop-color="#4a4b50"/><stop offset=".6" stop-color="#232427"/><stop offset="1" stop-color="#121214"/></radialGradient></defs>
    <path d="M${arcPt(-135, 41)} A41 41 0 1 1 ${arcPt(135, 41)}" fill="none" stroke="#0d0d0f" stroke-width="5" stroke-linecap="round"/>
    <path class="arc" fill="none" stroke="#f1f3f6" stroke-width="3.5" stroke-linecap="round"/>
    <circle cx="48" cy="48" r="31" fill="url(#g${key})" stroke="#000" stroke-width="2"/>
    <circle cx="48" cy="48" r="26" fill="none" stroke="rgba(255,255,255,.07)" stroke-width="1"/>
    <line class="ptr" x1="48" y1="27" x2="48" y2="38" stroke="#f1f3f6" stroke-width="3" stroke-linecap="round"/></svg></div>`), x, y);
  const val = lab('', x, y + 60, 'val');
  const arc = el.querySelector('.arc'), ptr = el.querySelector('.ptr');
  on(() => { const n = S[key].getNormalisedValue(), a = -135 + 270 * n;
    arc.setAttribute('d', n < 0.002 ? '' : `M${arcPt(-135, 41)} A41 41 0 ${a + 135 > 180 ? 1 : 0} 1 ${arcPt(a, 41)}`);
    ptr.setAttribute('transform', `rotate(${a} 48 48)`); val.textContent = fmt(get(key)); });
  let sy, sn;
  el.addEventListener('pointerdown', e => { el.setPointerCapture(e.pointerId); sy = e.clientY; sn = S[key].getNormalisedValue(); S[key].sliderDragStarted(); });
  el.addEventListener('pointermove', e => { if (sy == null) return; const k = e.shiftKey ? 0.0008 : 0.006;
    S[key].setNormalisedValue(Math.min(1, Math.max(0, sn + (sy - e.clientY) * k))); });
  el.addEventListener('pointerup', () => { sy = null; S[key].sliderDragEnded(); });
  el.addEventListener('dblclick', () => set(key, defs[key][2]));
  el.addEventListener('wheel', e => { e.preventDefault(); S[key].setNormalisedValue(Math.min(1, Math.max(0, S[key].getNormalisedValue() - Math.sign(e.deltaY) * 0.01))); }, { passive: false });
}
lab('INPUT', 103, 175);              knob('inGain', 103, 235, db);
lab('OUTPUT', 103, 362);             knob('outTrim', 103, 423, db);
lab('OUTPUT<br>GAIN', 963, 164);     knob('outGain', 963, 235, db);
lab('TONE', 963, 362);               knob('tone', 963, 423, v => Math.round(v * 100) + '%');
lab('Low-Pass Filter', 963, 513, 'sub');

/* ---------- pitch section ---------- */
const mkLeds = x => { const l = put($('<div class="leds abs"><b></b><b></b><b></b></div>'), x, 139); return [...l.children]; };
const dLeds = mkLeds(254 - 0), uLeds = mkLeds(813 - 0);
const mkStep = (cls, x, txt) => put($(`<div class="step abs"><i class="${cls}"></i>${txt ? '<span></span>' : ''}</div>`), x, 275);
const down = mkStep('d', 253, true), up = mkStep('u', 813, false);
lab('DOWN', 253, 178); lab('UP', 813, 178); lab('ADJUST', 253, 386, 'small'); lab('ADJUST', 813, 386, 'small');
const phase = put($('<div id="phase" class="abs">&Oslash;</div>'), 253, 462); lab('PHASE INVERT', 253, 513, 'small');
phase.onclick = () => PH.setValue(!PH.getValue());
const syncPhase = () => phase.classList.toggle('on', !!PH.getValue()); PH.valueChangedEvent.addListener(syncPhase); syncPhase();

const stepBy = (dir, fine) => { const v = get('pitch'), e = 1e-6;
  set('pitch', fine ? Math.round((v + dir * 0.1) * 10) / 10 : (dir > 0 ? Math.floor(v + e) + 1 : Math.ceil(v - e) - 1)); };
for (const [el, dir] of [[down, -1], [up, 1]]) { let t, r;
  el.addEventListener('pointerdown', e => { stepBy(dir, e.shiftKey); t = setTimeout(() => r = setInterval(() => stepBy(dir, e.shiftKey), 110), 420); });
  for (const ev of ['pointerup', 'pointerleave']) el.addEventListener(ev, () => { clearTimeout(t); clearInterval(r); }); }

const bezel = put($(`<div id="bezel" class="abs"><div id="meters"><span>Left</span><div class="m"></div><span>Right</span></div>
  <div class="side" style="left:20px"><u></u></div><div class="side" style="right:20px"><u></u></div>
  <div id="lcd"><i></i><div id="num"><span id="n">0.0</span><small>st</small></div><p>SEMITONES</p></div></div>`), 534, 254);
const mw = bezel.querySelector('.m'); mw.innerHTML = '<s></s>'.repeat(32);
const segs = [...mw.children], sides = [...bezel.querySelectorAll('.side u')];
lab('PITCH', 533, 406).id = 'pitchlab';
const lcd = bezel.querySelector('#lcd');
lcd.addEventListener('dblclick', () => set('pitch', 0));
lcd.addEventListener('wheel', e => { e.preventDefault(); stepBy(-Math.sign(e.deltaY), e.shiftKey); }, { passive: false });
let ly, lv; lcd.addEventListener('pointerdown', e => { lcd.setPointerCapture(e.pointerId); ly = e.clientY; lv = get('pitch'); });
lcd.addEventListener('pointermove', e => { if (ly != null) set('pitch', Math.round((lv + (ly - e.clientY) * (e.shiftKey ? 0.005 : 0.05)) * 100) / 100); });
lcd.addEventListener('pointerup', () => ly = null);

on(() => { const v = get('pitch'), r = Math.round(v * 10) / 10;
  bezel.querySelector('#n').textContent = (r > 0 ? '+' : '') + r.toFixed(1);
  down.querySelector('span').textContent = Math.max(0, -r).toFixed(1) + ' st';
  const n = Math.min(3, Math.ceil(Math.abs(v) / 4 - 1e-6));
  dLeds.forEach((l, i) => l.classList.toggle('on', v < 0 && i < n)); uLeds.forEach((l, i) => l.classList.toggle('on', v > 0 && i < n));
  const h = Math.abs(v) / 12 * 78.5; sides.forEach(u => { u.style.height = h + 'px'; u.style.top = (v >= 0 ? 78.5 - h : 78.5) + 'px'; }); });

/* meters: 16 segments per channel, -60..0 dBFS */
if (window.__JUCE__ && window.__JUCE__.backend) window.__JUCE__.backend.addEventListener('meter', e => {
  [e.l, e.r].forEach((lvl, ch) => { const n = Math.round(Math.max(0, Math.min(1, (20 * Math.log10(lvl + 1e-6) + 60) / 60)) * 16);
    for (let i = 0; i < 16; i++) segs[ch * 16 + i].classList.toggle('on', i < n); }); });
/* the two rows sit as a 2-row grid */
mw.style.gridTemplateColumns = 'repeat(16,9px)'; mw.style.gridAutoFlow = 'row';

/* ---------- presets / reset / midi ---------- */
const presets = { Default: 0, 'Octave Up': 12, 'Fifth Up': 7, 'Major Third Up': 4, 'Minor Third Down': -3, 'Fourth Down': -5, 'Octave Down': -12 };
const sel = document.getElementById('preset');
Object.keys(presets).forEach(n => sel.add(new Option(n, n)));
const resetAll = () => { for (const k in defs) set(k, defs[k][2]); PH.setValue(false); };
sel.onchange = () => set('pitch', presets[sel.value]);
document.getElementById('prev').onclick = () => { sel.selectedIndex = (sel.selectedIndex + sel.length - 1) % sel.length; sel.onchange(); };
document.getElementById('reset').onclick = () => { resetAll(); sel.value = 'Default'; };
document.getElementById('midi').onclick = e => e.currentTarget.classList.toggle('on');   // UI stub: MIDI mapping not wired yet
window.__started = true; if (hasJuce) stat.textContent = '';
