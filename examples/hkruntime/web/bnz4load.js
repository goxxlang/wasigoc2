// BNZ4 weights into Champ's weight heap. From bnz-loader/js/bnz-loader.js: the 12-byte header and
// JSON index first; verified segments cached in OPFS under bnz4/<sha256 of header+index>/; the
// missing ones fetched as coalesced HTTP Range runs in file order (4 at a time, at most `budget`
// bytes fetched but not yet decoded); methods 4 and 5 decoded on the GPU (bnz4dec.js), 0 and 1 on
// the CPU; every segment sha256-checked against the index, and a bad one dropped and fetched again
// on its own. Where bnz-loader copies into the runtime's GPU buffers, this lands each segment's raw
// bytes in the heap at raw_off: the guest reads the heap as model.safetensors (bnzshare.js openRaw)
// while later segments are still arriving, so its upload() is unchanged.
import { BNZ4Dec, sha256hex } from "./bnz4dec.js";
import { decompress } from "./fzstd.js";
import { notePages } from "./bnzshare.js";

const MAGIC = "BNZ4";
const KEEP = 3; // OPFS keeps the segments of this many models, the most recently used

async function rangeGet(url, a, n) {
  const r = await fetch(url, { headers: { Range: "bytes=" + a + "-" + (a + n - 1) }, cache: "no-store" });
  if (r.status !== 206) throw new Error("Range " + a + "+" + n + " answered " + r.status);
  const b = new Uint8Array(await r.arrayBuffer());
  if (b.length !== n) throw new Error("Range " + a + "+" + n + " gave " + b.length + " bytes");
  return b;
}

function zstd(b) {
  return decompress(b.byteOffset === 0 && b.byteLength === b.buffer.byteLength ? b : b.slice());
}

// The header and the JSON index: the heap's size comes from the index, so this runs first.
export async function readIndex(url) {
  const head = await rangeGet(url, 0, 12);
  if (String.fromCharCode(head[0], head[1], head[2], head[3]) !== MAGIC) throw new Error("not a BNZ4 file");
  const n = Number(new DataView(head.buffer).getBigUint64(4, true));
  if (!(n > 0 && n < 64 << 20)) throw new Error("BNZ4: index size " + n);
  const body = await rangeGet(url, 12, n);
  const prefix = new Uint8Array(12 + n);
  prefix.set(head);
  prefix.set(body, 12);
  const index = JSON.parse(new TextDecoder().decode(body));
  plan(index);
  return { prefix, index };
}

// The segments in file order, checked to tile [0, orig_size) exactly: the heap's ready flags count
// bytes, so a gap or an overlap would leave the guest waiting or reading a hole.
function plan(index) {
  const size = index && index.orig_size;
  if (!Number.isSafeInteger(size) || size <= 0) throw new Error("BNZ4: orig_size");
  const segs = (Array.isArray(index.segments) ? index.segments : []).map((s, i) => Object.assign({}, s, { i }));
  for (const s of segs) {
    for (const k of ["raw_off", "raw_len", "off", "len", "method"]) {
      if (!Number.isSafeInteger(s[k]) || s[k] < 0) throw new Error("BNZ4: segment " + s.i + " " + k);
    }
    if (!/^[0-9a-f]{64}$/.test(String(s.sha256 || ""))) throw new Error("BNZ4: segment " + s.i + " sha256");
  }
  segs.sort((a, b) => a.raw_off - b.raw_off);
  let at = 0;
  for (const s of segs) {
    if (s.raw_off !== at) throw new Error("BNZ4: the segments do not cover byte " + at);
    at += s.raw_len;
  }
  if (at !== size) throw new Error("BNZ4: the segments end at " + at + ", not " + size);
  return segs;
}

async function gpuDevice() {
  if (!navigator.gpu) throw new Error("BNZ4 needs WebGPU in this worker");
  const ad = await navigator.gpu.requestAdapter();
  if (!ad) throw new Error("BNZ4: no WebGPU adapter");
  return ad.requestDevice({
    requiredLimits: {
      maxStorageBufferBindingSize: ad.limits.maxStorageBufferBindingSize,
      maxBufferSize: ad.limits.maxBufferSize,
    },
  });
}

async function cacheTop() {
  if (!navigator.storage || !navigator.storage.getDirectory) return null;
  try {
    const root = await navigator.storage.getDirectory();
    return await root.getDirectoryHandle("bnz4", { create: true });
  } catch {
    return null;
  }
}

// After a full load: mark this model used, and drop all but the KEEP most recently used.
async function prune(top, keepName) {
  try {
    const mark = await (await top.getDirectoryHandle(keepName)).getFileHandle(".used", { create: true });
    const w = await mark.createWritable();
    await w.write(String(Date.now()));
    await w.close();
    const used = [];
    for await (const [name, h] of top.entries()) {
      if (h.kind !== "directory") continue;
      let t = 0;
      try { t = Number(await (await (await h.getFileHandle(".used")).getFile()).text()) || 0; } catch { t = 0; }
      used.push([name, t]);
    }
    used.sort((a, b) => b[1] - a[1]);
    for (const [name] of used.slice(KEEP)) {
      if (name !== keepName) await top.removeEntry(name, { recursive: true });
    }
  } catch { /* storage is a cache: a failure here only costs a later download */ }
}

// spec: { url, prefix (header + index bytes), step }; mem.buffer is the heap; table its page table;
// flags[i] is step i of the raw image: 0 missing, 1 ready, -1 failed.
export async function fillHeap4(spec, mem, table, flags, onProgress, opts) {
  const o = Object.assign({ concurrency: 4, maxRun: 32 << 20, budget: 192 << 20, retries: 3 }, opts || {});
  const prefix = spec.prefix;
  const index = JSON.parse(new TextDecoder().decode(prefix.subarray(12)));
  const segs = plan(index);
  const size = index.orig_size;
  const step = spec.step;
  const base = prefix.length;
  const win = spec.window || null;
  const viewLen = win ? win.bytes : size;
  const data = new Uint8Array(mem.buffer, 0, viewLen);
  const owners = win ? new Int32Array(win.owners) : null;
  const rel = win ? new Int32Array(win.ctrl) : null;
  const chunks = Math.ceil(size / step);
  const got = new Float64Array(chunks);

  function failAll() {
    for (let i = 0; i < chunks; i++) {
      if (Atomics.load(flags, i) === 0) {
        Atomics.store(flags, i, -1);
        Atomics.notify(flags, i);
      }
    }
  }
  function claim(stepIndex) {
    const slot = stepIndex % win.slots;
    for (;;) {
      const owner = Atomics.load(owners, slot);
      if (owner === stepIndex) return slot;
      const done = Atomics.load(rel, 0);
      if (owner < 0 || owner < done) {
        if (Atomics.compareExchange(owners, slot, owner, stepIndex) === owner) {
          if (owner >= 0) Atomics.store(flags, owner, 0);
          return slot;
        }
      } else {
        const seen = Atomics.load(rel, 0);
        Atomics.wait(rel, 0, seen, 120000);
      }
    }
  }
  function land(raw, off) {
    if (!win) {
      data.set(raw, off);
      notePages(table, off, raw.length, size);
    }
    const end = off + raw.length;
    for (let pos = off; pos < end; ) {
      const i = Math.floor(pos / step);
      const stop = Math.min(end, (i + 1) * step);
      const n = stop - pos;
      if (win) {
        const within = pos - i * step;
        const slot = claim(i);
        data.set(raw.subarray(pos - off, pos - off + n), slot * step + within);
        notePages(table, slot * step + within, n, win.bytes);
      }
      got[i] += n;
      if (got[i] >= Math.min(step, size - i * step) && Atomics.load(flags, i) !== 1) {
        Atomics.store(flags, i, 1);
        Atomics.notify(flags, i);
      }
      pos = stop;
    }
  }

  try {
    const dec = new BNZ4Dec(await gpuDevice(), zstd);
    const top = await cacheTop();
    const key = await sha256hex(prefix);
    let dir = null;
    if (top) {
      try { dir = await top.getDirectoryHandle(key, { create: true }); } catch { dir = null; }
    }
    const cachedFile = async (seg) => {
      if (!dir) return null;
      try {
        const f = await (await dir.getFileHandle(String(seg.i))).getFile();
        return f.size === seg.len ? f : null;
      } catch {
        return null;
      }
    };

    // What OPFS has (sizes only; the bytes are read when the segment's turn comes), and the
    // network runs for the rest, in file order.
    const fromCache = new Set();
    for (const s of segs) if (s.len && (await cachedFile(s))) fromCache.add(s.i);
    const wait = new Map();
    const runs = [];
    let run = null;
    for (const s of segs) {
      if (!s.len || fromCache.has(s.i)) continue;
      let resolve, reject;
      const p = new Promise((a, b) => { resolve = a; reject = b; });
      p.catch(() => {});
      wait.set(s.i, { p, resolve, reject });
      if (run && run.end === s.off && run.end - run.off + s.len <= o.maxRun) {
        run.segs.push(s);
        run.end += s.len;
      } else {
        run = { off: s.off, end: s.off + s.len, segs: [s] };
        runs.push(run);
      }
    }
    let inflight = 0;
    const waiters = [];
    const release = (n) => {
      inflight -= n;
      waiters.splice(0).forEach((w) => w());
    };
    let next = 0;
    const fetcher = async () => {
      while (next < runs.length) {
        const r = runs[next++];
        const n = r.end - r.off;
        while (inflight > 0 && inflight + n > o.budget) await new Promise((res) => waiters.push(res));
        inflight += n;
        try {
          const b = await rangeGet(spec.url, base + r.off, n);
          for (const s of r.segs) wait.get(s.i).resolve(b.subarray(s.off - r.off, s.off - r.off + s.len));
        } catch (e) {
          for (const s of r.segs) wait.get(s.i).reject(e);
        }
      }
    };
    Promise.all(Array.from({ length: o.concurrency }, fetcher)).catch(() => {});

    const total = segs.reduce((a, s) => a + s.len, 0) || 1;
    let done = 0;
    const writes = [];
    if (onProgress) onProgress(0);
    for (const seg of segs) {
      if (!seg.raw_len) continue;
      const fresh = !fromCache.has(seg.i);
      let payload;
      if (fresh) {
        payload = seg.len ? await wait.get(seg.i).p : new Uint8Array(0);
      } else {
        const f = await cachedFile(seg);
        payload = f ? new Uint8Array(await f.arrayBuffer()) : await rangeGet(spec.url, base + seg.off, seg.len);
      }
      for (let attempt = 0; ; attempt++) {
        try {
          const { gpu, raw } = await dec.decode(seg, payload, true);
          if (gpu) gpu.destroy();
          if (!raw || raw.length !== seg.raw_len || (await sha256hex(raw)) !== seg.sha256) {
            throw new Error(seg.name + ": sha256 mismatch");
          }
          land(raw, seg.raw_off);
          if ((fresh || attempt > 0) && dir) {
            const keep = payload;
            writes.push((async () => {
              const w = await (await dir.getFileHandle(String(seg.i), { create: true })).createWritable();
              await w.write(keep);
              await w.close();
            })().catch(() => {}));
          }
          break;
        } catch (e) {
          if (attempt >= o.retries) throw e;
          if (dir) {
            try { await dir.removeEntry(String(seg.i)); } catch { /* not cached */ }
          }
          payload = await rangeGet(spec.url, base + seg.off, seg.len);
        }
      }
      if (fresh) release(seg.len);
      done += seg.len;
      if (onProgress) onProgress(done / total);
    }
    await Promise.all(writes);
    if (top) await prune(top, key);
  } catch (err) {
    failAll();
    throw err;
  }
}
