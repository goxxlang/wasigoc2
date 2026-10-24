// Chunk delivery for a BNZ2 weight file. The container is the bnz codec.
// Each WebRTC blob is that codec's bytes, gzipped again. The join side
// gunzips, concatenates, and hands the BNZ2 bytes to openBytes.
import { openBytes } from "./bnz.js";

export const RANGE_STEP = 2 * 1024 * 1024;

// HeapVM page table: one byte per wasm page over one memory block
// (machwasm MachPageLookupTable + HvAllocateMemoryBlock). The block is a
// WebAssembly.Memory, the Chrome heap, not a JS ArrayBuffer of the file.
// The process row matches chrome.processes: private memory is this block,
// and jsMemory stays 0 because the body is not on the V8 heap.
export const HEAP_PAGE = 65536;

export function createWeightHeap(size) {
  const pages = Math.ceil(size / HEAP_PAGE);
  const mem = new WebAssembly.Memory({ initial: pages, maximum: pages, shared: true });
  const table = new SharedArrayBuffer(pages);
  return {
    mem,
    table,
    size,
    pages,
    proc: { id: 1, image: "model.bnz", type: "gpu", tpt: 1, privateMemory: 0, jsMemoryAllocated: 0, jsMemoryUsed: 0 },
  };
}

// A phone cannot allocate a shared wasm memory the size of the decoded
// safetensors image (Hodge-3.5 is 2.18 GB). The guest reads that image
// front to back, twice (the header, then the upload), so a window a little
// larger than the biggest segment is enough. owners[slot] is the file step
// currently stored there, or -1. ctrl[0] is the first file step the guest
// may still read; earlier steps can be overwritten.
export function openWeightWindow(maxRaw, step) {
  if (step <= 0 || maxRaw <= 0) throw new Error("weight window");
  let bytes = 64 * 1024 * 1024;
  const need = maxRaw + step;
  while (bytes < need) bytes += 64 * 1024 * 1024;
  if (bytes > 256 * 1024 * 1024) throw new Error("a weight segment is too large for this device");
  const heap = createWeightHeap(bytes);
  const slots = bytes / step;
  const ctrlSab = new SharedArrayBuffer(4);
  const ownersSab = new SharedArrayBuffer(slots * 4);
  new Int32Array(ownersSab).fill(-1);
  return { heap, bytes, slots, ctrlSab, ownersSab };
}

export function notePages(table, off, n, size) {
  const pages = new Uint8Array(table);
  const end = Math.min(size, off + n);
  if (end <= off) return;
  const a = Math.floor(off / HEAP_PAGE);
  const b = Math.floor((end - 1) / HEAP_PAGE);
  for (let i = a; i <= b; i++) pages[i] = 1;
}

export function privateMemory(table, size) {
  const pages = new Uint8Array(table);
  let n = 0;
  for (let i = 0; i < pages.length; i++) {
    if (pages[i] !== 1) continue;
    const start = i * HEAP_PAGE;
    n += Math.min(size, start + HEAP_PAGE) - start;
  }
  return n;
}

export function weightProcess(table, size) {
  return {
    id: 1,
    image: "model.bnz",
    type: "gpu",
    tpt: 1,
    privateMemory: privateMemory(table, size),
    jsMemoryAllocated: 0,
    jsMemoryUsed: 0,
  };
}

export const MAX_BLOB = 16 * 1024;
const FLAG_BLOB = 0x01;
const FLAG_GZIP = 0x02;
const ICE = { iceServers: [{ urls: "stun:stun.l.google.com:19302" }] };

export function encodeFrame(msg, blob, gzipped) {
  const js = new TextEncoder().encode(JSON.stringify(msg));
  const n = blob ? blob.length : 0;
  const out = new Uint8Array(5 + js.length + n);
  out[0] = (n ? FLAG_BLOB : 0) | (gzipped ? FLAG_GZIP : 0);
  new DataView(out.buffer).setUint32(1, js.length, false);
  out.set(js, 5);
  if (n) out.set(blob, 5 + js.length);
  return out;
}

export function decodeFrame(buf) {
  const u = buf instanceof Uint8Array ? buf : new Uint8Array(buf);
  if (u.length < 5) throw new Error("bnz: short frame");
  const n = new DataView(u.buffer, u.byteOffset, u.byteLength).getUint32(1, false);
  const msg = JSON.parse(new TextDecoder().decode(u.subarray(5, 5 + n)));
  const blob = (u[0] & FLAG_BLOB) ? u.subarray(5 + n) : new Uint8Array(0);
  return { msg, blob, gzip: (u[0] & FLAG_GZIP) !== 0 };
}

export async function gzipBytes(bytes) {
  const stream = new Blob([bytes]).stream().pipeThrough(new CompressionStream("gzip"));
  return new Uint8Array(await new Response(stream).arrayBuffer());
}

export async function gunzipBytes(bytes) {
  const stream = new Blob([bytes]).stream().pipeThrough(new DecompressionStream("gzip"));
  return new Uint8Array(await new Response(stream).arrayBuffer());
}

function sleep(ms) { return new Promise((r) => setTimeout(r, ms)); }

function waitGather(pc) {
  if (pc.iceGatheringState === "complete") return Promise.resolve();
  return new Promise((resolve) => {
    const t = setTimeout(resolve, 2500);
    pc.addEventListener("icegatheringstatechange", () => {
      if (pc.iceGatheringState === "complete") { clearTimeout(t); resolve(); }
    });
  });
}

async function pace(dc) {
  while (dc.bufferedAmount > 1024 * 1024) await sleep(5);
}

// Host: read model.bnz by HTTP Range and send gzip(bnz-slice) frames.
export function startHost({ url, room, onStatus }) {
  const base = roomBase(room);
  const pc = new RTCPeerConnection(ICE);
  const dc = pc.createDataChannel("bnz", { ordered: true });
  dc.binaryType = "arraybuffer";
  let size = 0;
  const say = (s) => { if (onStatus) onStatus(s); };

  dc.onopen = () => {
    dc.send(encodeFrame({ op: "hello", name: "bnz", proto: 1 }));
  };
  dc.onmessage = (ev) => {
    const frame = decodeFrame(ev.data);
    const m = frame.msg;
    if (m.op === "hello") {
      dc.send(encodeFrame({ op: "hello_ok", name: "bnz", proto: 1 }));
      return;
    }
    if (m.op === "stat") {
      dc.send(encodeFrame({ op: "stat_ok", id: m.id, path: m.path, size }));
      return;
    }
    if (m.op === "get") {
      sendRange(dc, url, m).catch((e) => {
        dc.send(encodeFrame({ op: "error", id: m.id, error: String(e.message || e) }));
      });
    }
  };

  (async () => {
    const head = await fetch(url, { method: "HEAD" });
    size = Number(head.headers.get("Content-Length"));
    if (!head.ok || !size) throw new Error("no weights");
    const offer = await pc.createOffer();
    await pc.setLocalDescription(offer);
    await waitGather(pc);
    await fetch(base + "/offer", { method: "POST", body: pc.localDescription.sdp });
    say("Waiting for a client.");
    for (;;) {
      const r = await fetch(base + "/answer");
      if (r.ok) {
        const sdp = await r.text();
        if (sdp.includes("v=0")) {
          await pc.setRemoteDescription({ type: "answer", sdp });
          say("Connected.");
          return;
        }
      }
      await sleep(200);
    }
  })().catch((e) => say(String(e.message || e)));

  return {
    close() { pc.close(); },
  };
}

async function readRange(url, off, n) {
  const r = await fetch(url, { headers: { Range: "bytes=" + off + "-" + (off + n - 1) } });
  if (r.status !== 206 && r.status !== 200) throw new Error("HTTP " + r.status);
  const b = new Uint8Array(await r.arrayBuffer());
  if (r.status === 206 || b.length === n) return b;
  return b.subarray(off, off + n);
}

async function sendRange(dc, url, m) {
  const end = m.off + m.size;
  dc.send(encodeFrame({ op: "get_ok", id: m.id, size: m.size }));
  for (let off = m.off; off < end; ) {
    const n = Math.min(MAX_BLOB, end - off);
    const raw = await readRange(url, off, n);
    const gz = await gzipBytes(raw);
    await pace(dc);
    dc.send(encodeFrame({ op: "chunk", id: m.id, off, last: off + n >= end }, gz, true));
    off += n;
  }
}

// Join: one offer check. No offer means the caller uses HTTP.
export async function pullWebRTC({ room, onProgress }) {
  const base = roomBase(room);
  const offerRes = await fetch(base + "/offer");
  if (!offerRes.ok) return null;
  const offer = await offerRes.text();
  if (!offer.includes("v=0")) return null;
  const pc = new RTCPeerConnection(ICE);
  const got = new Promise((resolve, reject) => {
    const timer = setTimeout(() => reject(new Error("weights timed out")), 15000);
    let seen = false;
    pc.ondatachannel = (ev) => {
      const dc = ev.channel;
      dc.binaryType = "arraybuffer";
      let out = null;
      let filled = 0;
      let chain = Promise.resolve();
      dc.onclose = () => {
        if (!out || filled < out.length) reject(new Error("weights closed"));
      };
      dc.onmessage = (msgEv) => {
        const frame = decodeFrame(msgEv.data);
        chain = chain.then(async () => {
          const m = frame.msg;
          if (m.op === "hello") {
            if (!seen) { seen = true; clearTimeout(timer); }
            dc.send(encodeFrame({ op: "hello_ok", name: "champ", proto: 1 }));
            dc.send(encodeFrame({ op: "stat", id: "s", path: "model.bnz" }));
            return;
          }
          if (m.op === "stat_ok") {
            out = new Uint8Array(m.size);
            dc.send(encodeFrame({ op: "get", id: "g", path: "model.bnz", off: 0, size: m.size }));
            return;
          }
          if (m.op === "error") throw new Error(m.error || "weights");
          if (m.op !== "chunk" || !out) return;
          const raw = frame.gzip ? await gunzipBytes(frame.blob) : frame.blob;
          out.set(raw, m.off);
          filled += raw.length;
          if (onProgress && out.length) onProgress(filled / out.length);
          if (m.last || filled >= out.length) {
            clearTimeout(timer);
            resolve(out);
          }
        }).catch((e) => { clearTimeout(timer); reject(e); });
      };
    };
  });
  await pc.setRemoteDescription({ type: "offer", sdp: offer });
  const answer = await pc.createAnswer();
  await pc.setLocalDescription(answer);
  await waitGather(pc);
  await fetch(base + "/answer", { method: "POST", body: pc.localDescription.sdp });
  try {
    return await got;
  } finally {
    pc.close();
  }
}

// The site serves the one model's packed bytes in gzip slices at a fixed endpoint: no path in
// the request, so it can only ever read that file.
const MODEL_PATH = "/models/champ/model.bnz";
const MODEL_BYTES = "/api/model/bytes";

// A WebRTC signaling room is a plain name, never a path.
function roomBase(room) {
  if (!/^[a-z0-9-]{1,32}$/.test(room || "")) throw new Error("weights");
  return "/api/bnz/" + room;
}

async function pullGzipApi(size, onProgress) {
  const probe = await fetch(MODEL_BYTES + "?off=0&n=1");
  if (!probe.ok) return null;
  const first = new Uint8Array(await probe.arrayBuffer());
  if (first.length !== 1) return null;
  const out = new Uint8Array(size);
  out.set(first, 0);
  let done = first.length;
  let next = 1;
  async function worker() {
    for (;;) {
      const off = next;
      next += MAX_BLOB;
      if (off >= size) return;
      const n = Math.min(MAX_BLOB, size - off);
      const r = await fetch(MODEL_BYTES + "?off=" + off + "&n=" + n);
      if (!r.ok) throw new Error("weights");
      const raw = new Uint8Array(await r.arrayBuffer());
      if (raw.length !== n) throw new Error("weights");
      out.set(raw, off);
      done += n;
      if (onProgress) onProgress(done / size);
    }
  }
  if (onProgress) onProgress(done / size);
  await Promise.all([worker(), worker(), worker(), worker()]);
  return out;
}

// One redirect, then ranges against the final URL. A 1 MiB range against
// the public link repeats the redirect for every piece.
const RANGE_WORKERS = 6;

function bodySize(res) {
  const cr = res.headers.get("Content-Range") || "";
  const total = Number((/\/(\d+)\s*$/.exec(cr) || [])[1]);
  const linked = Number(res.headers.get("X-Linked-Size"));
  const len = Number(res.headers.get("Content-Length"));
  return total || linked || len || 0;
}

async function pullRange(url, size, onProgress) {
  const out = new Uint8Array(size);
  let next = 0;
  let done = 0;
  async function worker() {
    for (;;) {
      const off = next;
      next += RANGE_STEP;
      if (off >= size) return;
      const n = Math.min(RANGE_STEP, size - off);
      const raw = await readRange(url, off, n);
      if (raw.length !== n) throw new Error("weights");
      out.set(raw, off);
      done += n;
      if (onProgress) onProgress(done / size);
    }
  }
  const jobs = [];
  for (let i = 0; i < RANGE_WORKERS; i++) jobs.push(worker());
  await Promise.all(jobs);
  return out;
}

export async function pullHttp(url, onProgress, knownSize) {
  const page = new URL(url, location.href);
  const same = page.origin === location.origin;
  let size = knownSize || 0;
  if (same && size && page.pathname === MODEL_PATH) {
    const gz = await pullGzipApi(size, onProgress);
    if (gz) return gz;
  }
  if (!size) {
    const probe = await fetch(url, { headers: { Range: "bytes=0-0" } });
    if (probe.status !== 206 && probe.status !== 200) throw new Error("weights");
    size = bodySize(probe);
    await probe.arrayBuffer();
    if (!size) throw new Error("weights");
    url = probe.url || url;
  }
  return pullRange(url, size, onProgress);
}

// Download into a SharedArrayBuffer the guest can read while bytes are
// still arriving. flags[i] is 0 missing, 1 ready, -1 failed.
const WEIGHT_CACHE = "champ-weights";

function chunkReq(etag, step, i) {
  return new Request("https://champ.invalid/bnz/" + etag + "/" + step + "/" + i);
}

async function openWeightCache(etag) {
  if (!etag || typeof caches === "undefined") return null;
  try { return await caches.open(WEIGHT_CACHE); }
  catch { return null; }
}

// Drop a previous Hugging Face file only after the new one is fully stored.
async function dropOtherWeights(cache, etag) {
  if (!cache || !etag) return;
  try {
    const keys = await cache.keys();
    await Promise.all(keys.map((req) => {
      if (!req.url.includes("/bnz/" + etag + "/")) return cache.delete(req);
    }));
  } catch { /* private mode or a full disk */ }
}

async function cachedChunk(cache, etag, step, i, n) {
  if (!cache || !etag) return null;
  try {
    const hit = await cache.match(chunkReq(etag, step, i));
    if (!hit) return null;
    const b = new Uint8Array(await hit.arrayBuffer());
    return b.length === n ? b : null;
  } catch {
    return null;
  }
}

function storeChunk(cache, etag, step, i, bytes) {
  if (!cache || !etag) return;
  const copy = bytes.slice();
  cache.put(chunkReq(etag, step, i), new Response(copy, {
    headers: { "content-length": String(copy.length) },
  })).catch(() => {});
}

function landChunk(data, table, flags, step, size, got, off, raw) {
  data.set(raw, off);
  notePages(table, off, raw.length, size);
  const i = Math.floor(off / step);
  got[i] += raw.length;
  const need = Math.min(step, size - i * step);
  if (got[i] >= need && Atomics.load(flags, i) !== 1) {
    Atomics.store(flags, i, 1);
    Atomics.notify(flags, i);
  }
}

async function streamWebRTC(spec, data, table, flags, step, onProgress) {
  const base = roomBase(spec.room);
  const offerRes = await fetch(base + "/offer");
  if (!offerRes.ok) return false;
  const offer = await offerRes.text();
  if (!offer.includes("v=0")) return false;
  const size = spec.size;
  const chunks = Math.ceil(size / step);
  const got = new Int32Array(chunks);
  const pc = new RTCPeerConnection(ICE);
  const done = new Promise((resolve, reject) => {
    const timer = setTimeout(() => reject(new Error("weights timed out")), 20000);
    let seen = false;
    pc.ondatachannel = (ev) => {
      const dc = ev.channel;
      dc.binaryType = "arraybuffer";
      let filled = 0;
      let chain = Promise.resolve();
      dc.onclose = () => {
        if (filled < size) reject(new Error("weights closed"));
      };
      dc.onmessage = (msgEv) => {
        const frame = decodeFrame(msgEv.data);
        chain = chain.then(async () => {
          const m = frame.msg;
          if (m.op === "hello") {
            if (!seen) { seen = true; clearTimeout(timer); }
            dc.send(encodeFrame({ op: "hello_ok", name: "champ", proto: 1 }));
            dc.send(encodeFrame({ op: "stat", id: "s", path: "model.bnz" }));
            return;
          }
          if (m.op === "stat_ok") {
            dc.send(encodeFrame({ op: "get", id: "g", path: "model.bnz", off: 0, size }));
            return;
          }
          if (m.op === "error") throw new Error(m.error || "weights");
          if (m.op !== "chunk") return;
          const raw = frame.gzip ? await gunzipBytes(frame.blob) : frame.blob;
          landChunk(data, table, flags, step, size, got, m.off, raw);
          filled += raw.length;
          if (onProgress && size) onProgress(filled / size);
          if (m.last || filled >= size) {
            clearTimeout(timer);
            resolve(true);
          }
        }).catch((e) => { clearTimeout(timer); reject(e); });
      };
    };
  });
  await pc.setRemoteDescription({ type: "offer", sdp: offer });
  const answer = await pc.createAnswer();
  await pc.setLocalDescription(answer);
  await waitGather(pc);
  await fetch(base + "/answer", { method: "POST", body: pc.localDescription.sdp });
  try {
    return await done;
  } finally {
    pc.close();
  }
}

async function streamRange(spec, data, table, flags, step, onProgress) {
  const size = spec.size;
  const chunks = Math.ceil(size / step);
  const got = new Int32Array(chunks);
  const cache = await openWeightCache(spec.etag || "");
  let next = 0;
  let done = 0;
  function fail(err) {
    for (let i = 0; i < chunks; i++) {
      if (Atomics.load(flags, i) === 0) {
        Atomics.store(flags, i, -1);
        Atomics.notify(flags, i);
      }
    }
    throw err;
  }
  async function worker() {
    for (;;) {
      const i = next;
      next += 1;
      if (i >= chunks) return;
      const off = i * step;
      const n = Math.min(step, size - off);
      let raw = await cachedChunk(cache, spec.etag, step, i, n);
      if (!raw) {
        try { raw = await readRange(spec.url, off, n); }
        catch (err) { fail(err); return; }
        if (raw.length !== n) { fail(new Error("weights")); return; }
        storeChunk(cache, spec.etag, step, i, raw);
      }
      landChunk(data, table, flags, step, size, got, off, raw);
      done += n;
      if (onProgress) onProgress(done / size);
    }
  }
  const jobs = [];
  for (let i = 0; i < RANGE_WORKERS; i++) jobs.push(worker());
  await Promise.all(jobs);
  await dropOtherWeights(cache, spec.etag || "");
}

// Chunks land in the wasm heap. A missing WebRTC host falls through to
// range reads of the same block. The JS side holds one chunk, not the file.
export async function fillHeap(spec, mem, table, flags, onProgress) {
  const data = new Uint8Array(mem.buffer, 0, spec.size);
  if (onProgress) onProgress(0);
  if (spec.room) {
    try {
      if (await streamWebRTC(spec, data, table, flags, spec.step, onProgress)) return;
    } catch { /* range fills the same heap */ }
  }
  await streamRange(spec, data, table, flags, spec.step, onProgress);
}

function flagView(flags) {
  return flags instanceof Int32Array ? flags : new Int32Array(flags);
}

export function openHeap(mem, flags, size, step) {
  const data = new Uint8Array(mem.buffer, 0, size);
  const words = flagView(flags);
  function ensure(off, len) {
    if (len <= 0) return;
    const last = Math.min(size, off + len);
    const a = Math.floor(off / step);
    const b = Math.floor((last - 1) / step);
    for (let i = a; i <= b; i++) {
      while (Atomics.load(words, i) === 0) {
        const woke = Atomics.wait(words, i, 0, 120000);
        if (woke === "timed-out" && Atomics.load(words, i) === 0) throw new Error("weights");
      }
      if (Atomics.load(words, i) !== 1) throw new Error("weights");
    }
  }
  return openBytes(data, ensure);
}

// A heap that holds the model's raw safetensors bytes (BNZ4: bnz4load.js lands each decoded,
// verified segment at its raw_off). readAt waits for the steps it touches, then copies them out.
// win, when set, is the phone window from openWeightWindow: the memory is only win.bytes long
// and file step i lives at slot i % win.slots. The guest reads twice from the start, so the
// first pass (the header) does not release steps; after the second read at 0, steps behind
// the cursor may be overwritten. A read larger than the window copies one step at a time.
export function openRaw(mem, flags, size, step, win, embed) {
  const words = flagView(flags);
  const span = win ? win.bytes : size;
  const data = new Uint8Array(mem.buffer, 0, span);
  let passes = 0;
  function release(stepIndex) {
    if (!win || passes < 2) return;
    if (Atomics.load(win.ctrl, 0) === stepIndex) return;
    Atomics.store(win.ctrl, 0, stepIndex);
    Atomics.notify(win.ctrl, 0);
  }
  function waitStep(i) {
    while (Atomics.load(words, i) !== 1) {
      if (Atomics.load(words, i) < 0) throw new Error("weights");
      const woke = Atomics.wait(words, i, 0, 120000);
      if (woke === "timed-out" && Atomics.load(words, i) !== 1) throw new Error("weights");
    }
  }
  // The phone path keeps the embedding tensor in a file. Bytes of that tensor
  // are copied there as the guest reads them. embed is {base, bytes, reuse, writeAt}.
  function tee(pos, bytes) {
    if (!embed || embed.reuse || !bytes || !bytes.length) return;
    const from = Math.max(pos, embed.base);
    const to = Math.min(pos + bytes.length, embed.base + embed.bytes);
    if (to <= from) return;
    if (!embed.writeAt(from - embed.base, bytes.subarray(from - pos, to - pos))) {
      throw new Error("embedding store write failed");
    }
  }
  return {
    size,
    readAt(pos, n) {
      if (pos < 0 || n <= 0) return new Uint8Array(0);
      const last = Math.min(size, pos + n);
      if (pos === 0) passes++;
      if (!win) {
        if (last > pos) {
          for (let i = Math.floor(pos / step); i <= Math.floor((last - 1) / step); i++) waitStep(i);
        }
        const out = data.slice(pos, last);
        tee(pos, out);
        return out;
      }
      const out = new Uint8Array(Math.max(0, last - pos));
      for (let at = pos; at < last;) {
        const stepIndex = Math.floor(at / step);
        release(stepIndex);
        waitStep(stepIndex);
        const slot = stepIndex % win.slots;
        if (Atomics.load(win.owners, slot) !== stepIndex) throw new Error("weights");
        const within = at - stepIndex * step;
        const ncopy = Math.min(last - at, step - within);
        out.set(data.subarray(slot * step + within, slot * step + within + ncopy), at - pos);
        at += ncopy;
      }
      tee(pos, out);
      return out;
    },
  };
}

export async function fillShared(url, sab, size, step, onProgress, etag) {
  const chunks = Math.ceil(size / step);
  const flags = new Int32Array(sab, 0, chunks);
  const data = new Uint8Array(sab, chunks * 4, size);
  const cache = await openWeightCache(etag || "");
  let next = 0;
  let done = 0;
  function fail(err) {
    for (let i = 0; i < chunks; i++) {
      if (Atomics.load(flags, i) === 0) {
        Atomics.store(flags, i, -1);
        Atomics.notify(flags, i);
      }
    }
    throw err;
  }
  async function worker() {
    for (;;) {
      const i = next;
      next += 1;
      if (i >= chunks) return;
      const off = i * step;
      const n = Math.min(step, size - off);
      let raw = await cachedChunk(cache, etag, step, i, n);
      if (!raw) {
        try { raw = await readRange(url, off, n); }
        catch (err) { fail(err); return; }
        if (raw.length !== n) { fail(new Error("weights")); return; }
        storeChunk(cache, etag, step, i, raw);
      }
      data.set(raw, off);
      Atomics.store(flags, i, 1);
      Atomics.notify(flags, i);
      done += n;
      if (onProgress) onProgress(done / size);
    }
  }
  const jobs = [];
  for (let i = 0; i < RANGE_WORKERS; i++) jobs.push(worker());
  await Promise.all(jobs);
  await dropOtherWeights(cache, etag || "");
}

export function openShared(sab, size, step) {
  const chunks = Math.ceil(size / step);
  const flags = new Int32Array(sab, 0, chunks);
  const data = new Uint8Array(sab, chunks * 4, size);
  function ensure(off, len) {
    if (len <= 0) return;
    const last = Math.min(size, off + len);
    const a = Math.floor(off / step);
    const b = Math.floor((last - 1) / step);
    for (let i = a; i <= b; i++) {
      while (Atomics.load(flags, i) === 0) {
        const woke = Atomics.wait(flags, i, 0, 120000);
        if (woke === "timed-out" && Atomics.load(flags, i) === 0) throw new Error("weights");
      }
      if (Atomics.load(flags, i) !== 1) throw new Error("weights");
    }
  }
  return openBytes(data, ensure);
}

// Bytes the guest can readAt. WebRTC (bnz + gzip) when a host is up,
// otherwise HTTP. The view decodes one BNZ segment at a time.
export async function openWeight(spec, onProgress) {
  let bytes = null;
  if (spec.room) {
    try { bytes = await pullWebRTC({ room: spec.room, onProgress }); }
    catch { bytes = null; }
  }
  if (!bytes) bytes = await pullHttp(spec.url, onProgress, spec.size || 0);
  return openBytes(bytes);
}
