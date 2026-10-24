// Champ worker. Same boot as WASMJsLoader/guest-worker.js, plus a BNZ weight
// view built on this thread before the guest starts. readAt stays here: a
// function cannot cross postMessage.
import { instantiate, run } from "/loader.js";
import * as gpu from "/webgpu/webgpu-exec.js";
import { createWeightHeap, fillHeap, openHeap, openRaw, openWeight, openWeightWindow, RANGE_STEP, weightProcess } from "./bnzshare.js";
import { readIndex } from "./bnz4load.js";

// One download worker filling `heap`; resolves at its first progress (the guest may start reading).
function heapWorker(script, msg, heap, size) {
  const dl = new Worker(new URL(script, import.meta.url), { type: "module" });
  const started = new Promise((resolve, reject) => {
    dl.onmessage = (ev) => {
      const m = ev.data || {};
      if (m.type === "progress") {
        self.postMessage({ type: "progress", p: m.p });
        resolve();
      } else if (m.type === "error") {
        self.postMessage({ type: "err", text: "weights: " + (m.text || "failed") });
        reject(new Error(m.text || "weights"));
      } else if (m.type === "done") {
        const proc = weightProcess(heap.table, size);
        self.postMessage({ type: "heap", privateMemory: proc.privateMemory, jsMemory: proc.jsMemoryUsed });
        resolve();
      }
    };
    dl.onerror = () => reject(new Error("weights"));
  });
  dl.postMessage(msg);
  return started;
}

function maxRawOf(index) {
  let n = 0;
  const segs = index.segments || [];
  for (let i = 0; i < segs.length; i++) if (segs[i].raw_len > n) n = segs[i].raw_len;
  return n;
}

// The embedding tensor is one contiguous run of decoded safetensors bytes.
function embedRange(index) {
  let base = -1;
  let bytes = 0;
  const segs = index.segments || [];
  for (let i = 0; i < segs.length; i++) {
    if (String(segs[i].name || "").indexOf("model.embed_tokens.weight") !== 0) continue;
    const off = segs[i].raw_off;
    if (base < 0 || off < base) base = off;
    bytes += segs[i].raw_len;
  }
  if (base < 0 || bytes <= 0) throw new Error("the checkpoint has no embedding table");
  return { base, bytes };
}

// The embedding table is 1.42 GB. A phone keeps it in OPFS and the guest reads
// one tile at a time, instead of allocating that table on the GPU. A matching
// stamp from a previous load skips writing it again.
async function prepareEmbed(files, sha, range) {
  const raw = files["model/config.json"];
  if (!raw) throw new Error("model config is missing");
  const cfg = JSON.parse(new TextDecoder().decode(raw));
  const vocab = cfg.vocab_size | 0;
  const hidden = cfg.hidden_size | 0;
  if (vocab < 1 || hidden < 1) throw new Error("model config has no vocabulary");
  if (cfg.tie_word_embeddings === false) throw new Error("this phone path needs tied embeddings");
  const bytes = vocab * hidden * 2;
  if (range.bytes !== bytes) throw new Error("embedding size does not match the checkpoint");
  if (!navigator.storage || !navigator.storage.getDirectory) throw new Error("This phone cannot store the embedding table.");
  const root = await navigator.storage.getDirectory();
  const fh = await root.getFileHandle("hodge-embed.bf16", { create: true });
  const markFh = await root.getFileHandle("hodge-embed.sha", { create: true });
  let handle, mark;
  try {
    handle = await fh.createSyncAccessHandle({ mode: "readwrite" });
    mark = await markFh.createSyncAccessHandle({ mode: "readwrite" });
  } catch (e) {
    try { handle = await fh.createSyncAccessHandle(); } catch { /* reported below */ }
    try { mark = await markFh.createSyncAccessHandle(); } catch { /* reported below */ }
    if (!handle || !mark) throw new Error("This phone cannot store the embedding table. " + (e && e.message ? e.message : ""));
  }
  let reuse = false;
  try {
    const got = new Uint8Array(mark.getSize());
    if (got.byteLength) mark.read(got, { at: 0 });
    reuse = handle.getSize() === bytes && new TextDecoder().decode(got) === sha;
  } catch { reuse = false; }
  if (!reuse) handle.truncate(bytes);
  let written = 0;
  let stamped = reuse;
  function stamp() {
    if (stamped) return;
    stamped = true;
    const b = new TextEncoder().encode(sha);
    mark.truncate(b.length);
    mark.write(b, { at: 0 });
    mark.flush();
    handle.flush();
  }
  files["model/embed.ready"] = new TextEncoder().encode(reuse ? "1" : "0");
  const store = {
    base: range.base,
    bytes,
    reuse,
    size: bytes,
    readAt(pos, n) {
      const out = new Uint8Array(n);
      let off = 0;
      while (off < n) {
        const k = handle.read(out.subarray(off), { at: pos + off });
        if (k <= 0) throw new Error("embedding read failed");
        off += k;
      }
      return out;
    },
    writeAt(pos, src) {
      let off = 0;
      while (off < src.length) {
        const k = handle.write(src.subarray(off), { at: pos + off });
        if (k <= 0) return false;
        off += k;
      }
      written += src.length;
      if (!reuse && written >= bytes) stamp();
      return true;
    },
    flush() { handle.flush(); },
  };
  files["model/embed.bf16"] = store;
  return store;
}

self.onmessage = async (e) => {
  const msg = e.data;
  if (msg && msg.type === "vgpu-canvas") {
    gpu.canvasAnswer(msg);
    return;
  }
  if (!msg || msg.type !== "boot") return;
  try {
    const files = msg.files || {};
    const w = files["model/model.safetensors"];
    const wasmTask = msg.bytes
      ? Promise.resolve(msg.bytes)
      : fetch(msg.wasm, { cache: "no-store" }).then((r) => r.arrayBuffer());
    let weight = null;
    let wasmBytes = null;
    if (w && w.codec === "bnz4" && w.url) {
      // BNZ4: the heap is the decoded safetensors image, filled segment by segment (bnz4fetch.js).
      const { prefix, index } = await readIndex(w.url);
      const size = index.orig_size;
      const step = RANGE_STEP;
      const heap = createWeightHeap(size);
      const flags = new SharedArrayBuffer(Math.ceil(size / step) * 4);
      const started = heapWorker("./bnz4fetch.js", {
        url: w.url, prefix, buf: heap.mem.buffer, table: heap.table, flags, step,
      }, heap, size);
      const ready = await Promise.all([wasmTask, started]);
      wasmBytes = ready[0];
      weight = openRaw(heap.mem, flags, size, step);
    } else if (w && w.codec === "bnz" && w.url && w.size) {
      const step = RANGE_STEP;
      const chunks = Math.ceil(w.size / step);
      const heap = createWeightHeap(w.size);
      const flags = new SharedArrayBuffer(chunks * 4);
      const dl = new Worker(new URL("./bnzfetch.js", import.meta.url), { type: "module" });
      const started = new Promise((resolve, reject) => {
        dl.onmessage = (ev) => {
          const m = ev.data || {};
          if (m.type === "progress") {
            self.postMessage({ type: "progress", p: m.p });
            resolve();
          } else if (m.type === "error") reject(new Error(m.text || "weights"));
          else if (m.type === "done") {
            const proc = weightProcess(heap.table, w.size);
            self.postMessage({ type: "heap", privateMemory: proc.privateMemory, jsMemory: proc.jsMemoryUsed });
            resolve();
          }
        };
        dl.onerror = () => reject(new Error("weights"));
      });
      dl.postMessage({
        url: w.url, buf: heap.mem.buffer, table: heap.table, flags, size: w.size, step,
        etag: w.etag || "", room: w.room || "",
      });
      const bytesEarly = await Promise.all([wasmTask, started]);
      weight = openHeap(heap.mem, flags, w.size, step);
      wasmBytes = bytesEarly[0];
    } else if (w && w.codec === "bnz") {
      weight = await openWeight(w, (p) => { self.postMessage({ type: "progress", p }); });
    }
    const bytes = wasmBytes || await wasmTask;
    if (weight) files["model/model.safetensors"] = weight;
    const inst = await instantiate(bytes, {
      args: msg.args || ["guest.wasm"],
      env: msg.env || [],
      files,
      stdinSab: msg.sab || null,
      stdout: (t) => self.postMessage({ type: "out", text: t }),
      stderr: (t) => self.postMessage({ type: "err", text: t }),
      webgpu: gpu,
    });
    const code = await run(inst);
    self.postMessage({ type: "exit", code });
  } catch (err) {
    self.postMessage({ type: "err", text: err && err.stack ? err.stack : String(err) });
    self.postMessage({ type: "exit", code: -1 });
  }
};

self.postMessage({ type: "worker-ready" });
