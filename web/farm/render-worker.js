// Renders part of a song to audio off the page's main thread and away from the audio thread, with the same native
// engine (awdsp.wasm) that plays it live. The page posts the location, where to start and how long to render; the
// worker sends back the left and right channels.
let wasm = null;

async function load() {
  if (wasm) return wasm;
  const bytes = await (await fetch('awdsp.wasm')).arrayBuffer();
  const ok = () => 0;
  const { instance } = await WebAssembly.instantiate(bytes, { wasi_snapshot_preview1: { fd_close: ok, fd_seek: ok, fd_write: ok, proc_exit: ok } });
  wasm = instance.exports;
  if (wasm._initialize) wasm._initialize();
  return wasm;
}

function cstr(x, text) {
  const u8 = new TextEncoder().encode(text), p = x.malloc(u8.length + 1), m = new Uint8Array(x.memory.buffer);
  m.set(u8, p);
  m[p + u8.length] = 0;
  return p;
}

onmessage = async e => {
  const m = e.data;
  try {
    const x = await load(), rate = m.rate || 44100, e2 = x.aw_engine_create(rate);
    for (const s of m.samples || []) {
      const id = cstr(x, s.id), p = x.malloc(s.data.length * 4);
      new Float32Array(x.memory.buffer, p, s.data.length).set(s.data);
      x.aw_engine_add_sample(e2, id, p, s.data.length, s.rate);
      x.free(p); x.free(id);
    }
    const l = cstr(x, m.location), ms = cstr(x, m.master);
    const r = x.aw_engine_set_location(e2, l, ms);
    x.free(l); x.free(ms);
    if (r !== 0) throw new Error('The engine could not read the song.');
    const c = m.context, ctx = x.malloc(20);
    new Int32Array(x.memory.buffer, ctx, 5).set([c.level, c.song, c.part, c.bar, c.loop ? 1 : 0]);
    x.aw_engine_play_count_in(e2, ctx, 0);
    const total = Math.ceil(m.seconds * rate), stopAt = Math.ceil(m.music * rate), block = 4096;
    const L = new Float32Array(total), R = new Float32Array(total), pl = x.malloc(block * 4), pr = x.malloc(block * 4);
    let stopped = false;
    for (let i = 0, n = 0; i < total; i += n) {
      // the music stops where the range ends; what's still ringing (releases, echoes, reverb) plays out
      if (!stopped && i >= stopAt) { x.aw_engine_stop(e2); stopped = true; }
      n = Math.min(block, total - i, i < stopAt ? stopAt - i : block);
      x.aw_engine_render(e2, n, pl, pr);
      L.set(new Float32Array(x.memory.buffer, pl, n), i);
      R.set(new Float32Array(x.memory.buffer, pr, n), i);
    }
    x.free(pl); x.free(pr); x.free(ctx);
    x.aw_engine_destroy(e2);
    postMessage({ id: m.id, L, R, rate }, [L.buffer, R.buffer]);
  } catch (err) {
    postMessage({ id: m.id, error: String(err && err.message || err) });
  }
};
