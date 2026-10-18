// Renders a song from the web prototype to a WAV file, for comparing against the native engine (aw-render).
// The prototype runs in headless Chromium with its AudioContext swapped for an OfflineAudioContext; its own
// sequencer schedules the whole song against a fake clock, then the context renders it.
//
// Usage, from the repo root (needs Playwright with Chromium; NODE_PATH pointing at a global install works):
//   node tools/parity/render-prototype.js out.wav [--song 0] [--level 0] [--part 0] [--bar 0] [--seconds 20] [--solo N] [--state save.json]
// The prototype starts playback 80 ms in; that lead-in is trimmed so the file lines up with aw-render's.
const path = require('path');
const fs = require('fs');
const { chromium } = require('playwright');

const args = process.argv.slice(2);
const out = args[0];
const opt = (name, def) => {
  const i = args.indexOf('--' + name);
  return i >= 0 ? Number(args[i + 1]) : def;
};
const song = opt('song', 0), level = opt('level', 0), part = opt('part', 0), bar = opt('bar', 0);
const seconds = opt('seconds', 20), rate = 48000, solo = opt('solo', -1);
const si = args.indexOf('--state'), stateText = si >= 0 ? fs.readFileSync(args[si + 1], 'utf8') : null;

function writeWav(file, L, R, sr) {
  const n = L.length, buf = Buffer.alloc(44 + n * 4);
  buf.write('RIFF', 0); buf.writeUInt32LE(36 + n * 4, 4); buf.write('WAVE', 8);
  buf.write('fmt ', 12); buf.writeUInt32LE(16, 16); buf.writeUInt16LE(1, 20); buf.writeUInt16LE(2, 22);
  buf.writeUInt32LE(sr, 24); buf.writeUInt32LE(sr * 4, 28); buf.writeUInt16LE(4, 32); buf.writeUInt16LE(16, 34);
  buf.write('data', 36); buf.writeUInt32LE(n * 4, 40);
  for (let i = 0; i < n; i++) {
    for (const [c, x] of [[0, L[i]], [1, R[i]]]) {
      const v = Math.max(-1, Math.min(1, x));
      buf.writeInt16LE(Math.round(v * 32767), 44 + i * 4 + c * 2);
    }
  }
  fs.writeFileSync(file, buf);
}

(async () => {
  if (!out) {
    console.error('usage: render-prototype.js out.wav [--song N] [--level N] [--part N] [--bar N] [--seconds S]');
    process.exit(2);
  }
  const browser = await chromium.launch();
  const page = await browser.newPage();
  const errors = [];
  page.on('pageerror', e => errors.push(String(e)));
  await page.goto('file://' + path.resolve(__dirname, '../../reference/fractal-groovebox.html'));
  await page.waitForTimeout(1500);
  const result = await page.evaluate(async ({ song, level, part, bar, seconds, rate, solo, stateText }) => {
    if (stateText) { state = JSON.parse(stateText); bindSongs(state); }
    const lead = 0.08;
    const off = new OfflineAudioContext(2, Math.ceil((seconds + lead) * rate), rate);
    let fake = 0;
    Object.defineProperty(off, 'currentTime', { get: () => fake });
    window.AudioContext = function () { return off; };
    window.webkitAudioContext = undefined;
    ac = null;
    sel[0] = song; sel[1] = part; sel[4] = bar; sel[3] = null;
    z = level;
    if (solo >= 0) state.songs[song].mix.s[solo] = 1;
    play();
    clearInterval(timer);
    for (fake = 0; fake < seconds + lead; fake += 0.025) tick();
    const buf = await off.startRendering();
    const skip = Math.round(lead * rate), n = Math.round(seconds * rate);
    const pack = ch => {
      const d = buf.getChannelData(ch).slice(skip, skip + n);
      let s = '';
      const b = new Uint8Array(d.buffer);
      for (let i = 0; i < b.length; i += 0x8000) s += String.fromCharCode.apply(null, b.subarray(i, i + 0x8000));
      return btoa(s);
    };
    return { L: pack(0), R: pack(1), name: state.songs[song].name };
  }, { song, level, part, bar, seconds, rate, solo, stateText });
  await browser.close();
  if (errors.length) console.error('page errors:', errors);
  const decode = b => new Float32Array(new Uint8Array(Buffer.from(b, 'base64')).buffer);
  const L = decode(result.L), R = decode(result.R);
  let peak = 0, sum = 0;
  for (let i = 0; i < L.length; i++) { peak = Math.max(peak, Math.abs(L[i]), Math.abs(R[i])); sum += L[i] * L[i] + R[i] * R[i]; }
  console.log(`${result.name}: ${L.length} frames, peak ${peak.toFixed(3)} (${(20 * Math.log10(peak)).toFixed(1)} dBFS), rms ${(10 * Math.log10(sum / (2 * L.length))).toFixed(1)} dBFS`);
  writeWav(out, L, R, rate);
})();
