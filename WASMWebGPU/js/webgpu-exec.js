// WebGPU's hands in the JS host: a module's webgpu.h (WASMWebGPU/src/remote.cc)
// hands a batch of commands to its suspending import (webgpu.batch; HeapVM's
// heap.gpu), and this runs them against the host's own navigator.gpu. A batch
// that awaits (an adapter, a device, a mapped range) suspends the module until
// the promise settles; a batch that does not is run and answered at once. A
// page is asked only for a display: an OffscreenCanvas it transfers here
// (answered with canvasAnswer).
//
// A batch is u32 jsonLen | JSON commands | blob. The reply is
// u32 textLen | `key=value` lines | bytes.

const objs = new Map(); // module id -> GPU object
const mapped = new Map(); // buffer id -> its mapped ArrayBuffer
const pulled = new Map(); // buffer id -> bytes of a map the module is reading
const canvases = new Map(); // surface id -> OffscreenCanvas
const displays = new Map(); // selector -> Promise<OffscreenCanvas>, one transfer each
const waiting = new Map(); // selector -> {resolve, reject} for the page's answer
const pending = []; // uncaptured errors and lost devices, for the next reply

function esc(v) {
  return String(v).replace(/\\/g, "\\\\").replace(/\n/g, "\\n").replace(/\t/g, "\\t");
}

function errText(e) {
  return e && e.message ? e.message : String(e);
}

function errorKind(e) {
  return typeof GPUValidationError !== "undefined" && e instanceof GPUValidationError
    ? "validation"
    : typeof GPUOutOfMemoryError !== "undefined" && e instanceof GPUOutOfMemoryError
    ? "out-of-memory"
    : "internal";
}

// Arguments: {"$":id} is an object, {"#":[off,len]} blob bytes, {"u":1} a
// left-out optional argument.
function resolve(v, blob) {
  if (Array.isArray(v)) return v.map((x) => resolve(x, blob));
  if (v && typeof v === "object") {
    if ("$" in v && Object.keys(v).length === 1) return objs.get(v.$);
    if ("#" in v && Object.keys(v).length === 1) return blob.subarray(v["#"][0], v["#"][0] + v["#"][1]);
    if ("u" in v && Object.keys(v).length === 1) return undefined;
    const out = {};
    for (const k of Object.keys(v)) out[k] = resolve(v[k], blob);
    return out;
  }
  return v;
}

// The display for `sel`: the page transfers its canvas once; every surface
// on that selector draws to it after.
function display(sel) {
  let p = displays.get(sel);
  if (!p) {
    p = new Promise((res, rej) => waiting.set(sel, { resolve: res, reject: rej }));
    displays.set(sel, p);
    self.postMessage({ type: "vgpu-canvas", sel });
  }
  return p;
}

// The page's answer to a vgpu-canvas request (shell-worker.js onmessage).
export function canvasAnswer(msg) {
  const w = waiting.get(msg.sel);
  if (!w) return;
  waiting.delete(msg.sel);
  if (msg.canvas) {
    w.resolve(msg.canvas);
  } else {
    displays.delete(msg.sel);
    w.reject(new Error(msg.error || "no canvas for " + msg.sel));
  }
}

function watchDevice(id, device) {
  device.addEventListener("uncapturederror", (ev) => {
    const e = ev.error;
    pending.push("uncaptured=" + esc(id + "\t" + errorKind(e) + "\t" + (e && e.message ? e.message : "error")));
  });
  device.lost.then((info) => {
    pending.push("lost=" + esc(id + "\t" + (info.reason || "unknown") + "\t" + (info.message || "")));
  });
}

function capsLines(lines, features, limits) {
  for (const f of features) lines.push("feature=" + esc(f));
  for (const k in limits) {
    const v = limits[k];
    if (typeof v === "number") lines.push("limit." + k + "=" + v);
  }
}

// One command. Returns a promise only when the command has to await.
function step(cmd, blob, lines, out) {
  switch (cmd.o) {
    case "new": {
      const parent = cmd.t ? objs.get(cmd.t) : null;
      const args = resolve(cmd.a, blob);
      const made = parent[cmd.m](...args);
      objs.set(cmd.id, made);
      if (cmd.m === "createBuffer" && args[0] && args[0].mappedAtCreation) {
        mapped.set(cmd.id, made.getMappedRange());
      }
      return;
    }
    case "call": {
      const target = objs.get(cmd.t);
      if (cmd.m === "destroy") mapped.delete(cmd.t);
      target[cmd.m](...resolve(cmd.a, blob));
      return;
    }
    case "set":
      objs.get(cmd.t)[cmd.k] = cmd.v;
      return;
    case "drop":
      objs.delete(cmd.t);
      mapped.delete(cmd.t);
      pulled.delete(cmd.t);
      return;
    case "wgsl":
      if (navigator.gpu && navigator.gpu.wgslLanguageFeatures)
        for (const f of navigator.gpu.wgslLanguageFeatures) lines.push("wgsl=" + esc(f));
      return;
    case "adapter":
      return (async () => {
        if (!navigator.gpu) {
          lines.push("unavailable=1");
          throw new Error("WebGPU is not available in this worker (navigator.gpu is missing)");
        }
        const adapter = await navigator.gpu.requestAdapter(...resolve(cmd.a, blob));
        if (!adapter) {
          lines.push("unavailable=1");
          throw new Error("no WebGPU adapter");
        }
        objs.set(cmd.id, adapter);
        const info = adapter.info || {};
        lines.push("vendor=" + esc(info.vendor || ""));
        lines.push("architecture=" + esc(info.architecture || ""));
        lines.push("device=" + esc(info.device || ""));
        lines.push("description=" + esc(info.description || ""));
        if (info.subgroupMinSize) lines.push("subgroupMinSize=" + info.subgroupMinSize);
        if (info.subgroupMaxSize) lines.push("subgroupMaxSize=" + info.subgroupMaxSize);
        if (info.isFallbackAdapter) lines.push("fallback=1");
        capsLines(lines, adapter.features, adapter.limits);
      })();
    case "device":
      return (async () => {
        const adapter = objs.get(cmd.t);
        const device = await adapter.requestDevice(...resolve(cmd.a, blob));
        objs.set(cmd.id, device);
        objs.set(cmd.q, device.queue);
        watchDevice(cmd.id, device);
        capsLines(lines, device.features, device.limits);
      })();
    case "newAsync":
      return (async () => {
        const parent = objs.get(cmd.t);
        try {
          objs.set(cmd.id, await parent[cmd.m](...resolve(cmd.a, blob)));
        } catch (e) {
          if (e && e.reason) lines.push("reason=" + esc(e.reason));
          throw e;
        }
      })();
    case "map":
      return (async () => {
        const buf = objs.get(cmd.t);
        await buf.mapAsync(cmd.mode, cmd.off, cmd.size);
        const range = buf.getMappedRange(cmd.off, cmd.size);
        mapped.set(cmd.t, range);
        const bytes = new Uint8Array(range.slice(0));
        pulled.set(cmd.t, bytes);
        lines.push("total=" + bytes.length);
        out.bin = bytes.subarray(0, Math.min(bytes.length, 192 * 1024));
      })();
    case "pull": {
      const bytes = pulled.get(cmd.t);
      if (!bytes) throw new Error("the buffer is not mapped");
      out.bin = bytes.subarray(cmd.from, cmd.from + cmd.len);
      return;
    }
    case "unmap": {
      const buf = objs.get(cmd.t);
      const range = mapped.get(cmd.t);
      if (cmd.w && range) new Uint8Array(range).set(blob.subarray(cmd.w["#"][0], cmd.w["#"][0] + cmd.w["#"][1]));
      mapped.delete(cmd.t);
      pulled.delete(cmd.t);
      buf.unmap();
      return;
    }
    case "done":
      return objs.get(cmd.t).onSubmittedWorkDone();
    case "pop":
      return objs
        .get(cmd.t)
        .popErrorScope()
        .then((e) => {
          if (!e) {
            lines.push("type=none");
            return;
          }
          lines.push("type=" + errorKind(e));
          lines.push("msg=" + esc(e.message || ""));
        });
    case "info":
      return objs
        .get(cmd.t)
        .getCompilationInfo()
        .then((info) => {
          for (const m of info.messages)
            lines.push(
              "msg=" + esc([m.type, m.lineNum, m.linePos, m.offset, m.length, m.message].join("\t"))
            );
        });
    case "surface":
      return (async () => {
        const canvas = await display(cmd.sel);
        const ctx = canvas.getContext("webgpu");
        if (!ctx) throw new Error("no webgpu context on " + cmd.sel);
        canvases.set(cmd.id, canvas);
        objs.set(cmd.id, ctx);
        lines.push("preferred=" + esc(navigator.gpu.getPreferredCanvasFormat()));
      })();
    case "canvas": {
      const canvas = canvases.get(cmd.t);
      if (canvas && cmd.w && cmd.h) {
        canvas.width = cmd.w;
        canvas.height = cmd.h;
      }
      return;
    }
    default:
      throw new Error("unknown vgpu command " + cmd.o);
  }
}

function failed(st, cmd, e) {
  st.lines.push("error=" + esc(cmd.o + (cmd.m ? " " + cmd.m : "") + ": " + errText(e)));
}

// Runs the batch from command st.i on. Stays synchronous until a command
// returns a promise; the rest of the batch then runs after it settles.
function drive(st) {
  while (st.i < st.cmds.length) {
    const cmd = st.cmds[st.i++];
    let r;
    try {
      r = step(cmd, st.blob, st.lines, st.out);
    } catch (e) {
      failed(st, cmd, e);
      continue;
    }
    if (r && typeof r.then === "function") {
      return r.then(
        () => drive(st),
        (e) => {
          failed(st, cmd, e);
          return drive(st);
        }
      );
    }
  }
  while (pending.length) st.lines.push(pending.shift());
  return { text: st.lines.join("\n"), bin: st.out.bin };
}

// One batch: {text, bin}, or a promise of it when a command awaited.
// `bytes` is a view of the module's memory; it is only read before the
// module resumes, and WebGPU copies what it keeps.
export function runBatch(bytes) {
  const jlen = new DataView(bytes.buffer, bytes.byteOffset, 4).getUint32(0, true);
  const cmds = JSON.parse(new TextDecoder().decode(bytes.subarray(4, 4 + jlen)));
  const st = { cmds, blob: bytes.subarray(4 + jlen), lines: [], out: { bin: null }, i: 0 };
  return drive(st);
}

// The reply, as webgpu_remote.cc reads it, into [dst, dst + cap). Its size.
export function writeReply(reply, dst) {
  let text = new TextEncoder().encode(reply.text);
  let bin = reply.bin || new Uint8Array(0);
  if (4 + text.length + bin.length > dst.length) {
    text = new TextEncoder().encode(
      "error=vgpu: reply too large (" + (4 + text.length + bin.length) + " bytes)"
    );
    bin = new Uint8Array(0);
  }
  new DataView(dst.buffer, dst.byteOffset, 4).setUint32(0, text.length, true);
  dst.set(text, 4);
  dst.set(bin, 4 + text.length);
  return 4 + text.length + bin.length;
}
