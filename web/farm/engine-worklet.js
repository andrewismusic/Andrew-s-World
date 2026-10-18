// The native sound engine (awdsp.wasm) running in the page's audio thread. The page posts song data and
// transport commands; the worklet posts back clock ticks and note starts with their times on the AudioContext clock.
class EngineProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
    this.x = null;
    this.base = null;
    this.port.onmessage = e => { try { this.handle(e.data); } catch (err) { this.port.postMessage({ t: 'error', message: String(err && err.message || err) }); } };
  }

  init(bytes) {
    const ok = () => 0;
    const imports = { wasi_snapshot_preview1: { fd_close: ok, fd_seek: ok, fd_write: ok, proc_exit: ok } };
    const inst = new WebAssembly.Instance(new WebAssembly.Module(bytes), imports);
    const x = this.x = inst.exports;
    if (x._initialize) x._initialize();
    this.eng = x.aw_engine_create(sampleRate);
    this.L = x.malloc(512 * 4);
    this.R = x.malloc(512 * 4);
    this.ctx = x.malloc(20);
    this.cap = 256;
    this.tickBuf = x.malloc(32 * this.cap);
    this.noteBuf = x.malloc(32 * this.cap);
    this.port.postMessage({ t: 'ready', sampleRate });
  }

  bytes(u8) {
    const p = this.x.malloc(u8.length + 1);
    const m = new Uint8Array(this.x.memory.buffer);
    m.set(u8, p);
    m[p + u8.length] = 0;
    return p;
  }

  context(c) {
    new Int32Array(this.x.memory.buffer, this.ctx, 5).set([c.level, c.song, c.part, c.bar, c.loop ? 1 : 0]);
    return this.ctx;
  }

  handle(m) {
    if (m.t === 'init') return this.init(m.wasm);
    const x = this.x;
    if (!x) return;
    const e = this.eng;
    switch (m.t) {
      case 'location': {
        const l = this.bytes(m.location), s = this.bytes(m.master);
        const r = x.aw_engine_set_location(e, l, s);
        x.free(l); x.free(s);
        if (r !== 0) this.port.postMessage({ t: 'error', message: 'The engine could not read the song data.' });
        break;
      }
      case 'play': x.aw_engine_play_count_in(e, this.context(m.context), m.countIn | 0); break;
      case 'stop': x.aw_engine_stop(e); break;
      case 'context': x.aw_engine_set_context(e, this.context(m.context)); break;
      case 'cue': x.aw_engine_cue(e, m.song, m.part, m.bar); break;
      case 'uncue': x.aw_engine_cancel_cue(e); break;
      case 'audition': x.aw_engine_audition(e, m.id); break;
      case 'liveOn': x.aw_engine_live_on(e, m.id, m.song, m.part, m.slot, m.pitch); break;
      case 'liveOff': x.aw_engine_live_off(e, m.id); break;
      case 'metronome': x.aw_engine_metronome(e, m.on ? 1 : 0, m.ticks, m.volume); break;
      case 'sample': {
        const id = this.bytes(m.id), n = m.data.length, p = x.malloc(n * 4);
        new Float32Array(x.memory.buffer, p, n).set(m.data);
        x.aw_engine_add_sample(e, id, p, n, m.rate);
        x.free(p); x.free(id);
        break;
      }
    }
  }

  process(inputs, outputs) {
    const x = this.x, out = outputs[0];
    if (!x || !out || !out[0]) return true;
    const n = out[0].length;
    if (this.base === null) this.base = currentTime;
    x.aw_engine_render(this.eng, n, this.L, this.R);
    const mem = x.memory.buffer;
    out[0].set(new Float32Array(mem, this.L, n));
    if (out[1]) out[1].set(new Float32Array(mem, this.R, n));

    const nt = x.aw_engine_take_ticks(this.eng, this.tickBuf, this.cap);
    const nn = x.aw_engine_take_notes(this.eng, this.noteBuf, this.cap);
    if (nt || nn) {
      const dv = new DataView(x.memory.buffer), ticks = [], notes = [];
      for (let i = 0; i < nt; i++) {
        const o = this.tickBuf + i * 32;
        ticks.push({ t: this.base + dv.getFloat64(o, true), s: dv.getInt32(o + 8, true), c: dv.getInt32(o + 12, true),
          p: dv.getInt32(o + 16, true), tk: dv.getInt32(o + 20, true), bt: dv.getInt32(o + 24, true) });
      }
      for (let i = 0; i < nn; i++) {
        const o = this.noteBuf + i * 32, t0 = this.base + dv.getFloat64(o, true);
        notes.push({ t0, t1: t0 + dv.getFloat64(o + 8, true), id: dv.getInt32(o + 16, true), slot: dv.getInt32(o + 20, true), pitch: dv.getInt32(o + 24, true) });
      }
      this.port.postMessage({ t: 'events', ticks, notes });
    }
    return true;
  }
}

registerProcessor('aw-engine', EngineProcessor);
