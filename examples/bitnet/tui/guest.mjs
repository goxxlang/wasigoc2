// Worker: Dawn's WebGPU, then the existing bitnet.wasm chat (no -ui).
import { workerData, parentPort } from "node:worker_threads";
import { openSync, readSync, fstatSync, readFileSync, readdirSync } from "node:fs";
import { join } from "node:path";
import { create, globals } from "webgpu";
import { instantiate, run } from "../../../WASMJsLoader/loader.js";
import * as gpu from "../../../WASMWebGPU/js/webgpu-exec.js";

Object.assign(globalThis, globals);
const dawn = create([]);
// The guest chunks the embedding by the binding limit, then binds the matching
// logit slice at 4 bytes per row. Dawn requires that offset to be a multiple
// of 256, so the row count must be a multiple of 64. One embedding row is
// 5120 bytes, and 64 of those is the unit the limit is snapped down to.
function capAdapter(adapter) {
  const lim = adapter.limits;
  const unit = 5120 * 64;
  const room = Math.min(Number(lim.maxBufferSize), Number(lim.maxStorageBufferBindingSize));
  const snapped = Math.floor(room / unit) * unit;
  return new Proxy(adapter, {
    get(t, key) {
      if (key === "requestDevice") {
        return (desc) => {
          const next = Object.assign({}, desc);
          const required = Object.assign({}, next.requiredLimits);
          required.maxBufferSize = snapped;
          required.maxStorageBufferBindingSize = snapped;
          next.requiredLimits = required;
          return t.requestDevice(next);
        };
      }
      const v = t[key];
      return typeof v === "function" ? v.bind(t) : v;
    },
  });
}
const gpuHost = {
  requestAdapter: async (...args) => {
    const adapter = await dawn.requestAdapter(...args);
    return adapter ? capAdapter(adapter) : adapter;
  },
  get wgslLanguageFeatures() {
    return dawn.wgslLanguageFeatures;
  },
  getPreferredCanvasFormat() {
    return dawn.getPreferredCanvasFormat();
  },
};
const nav = globalThis.navigator;
if (nav && typeof nav === "object") {
  Object.defineProperty(nav, "gpu", { value: gpuHost, configurable: true });
} else {
  Object.defineProperty(globalThis, "navigator", { value: { gpu: gpuHost }, configurable: true });
}

function disk(path) {
  const fd = openSync(path, "r");
  const size = fstatSync(fd).size;
  return {
    size,
    readAt(pos, n) {
      const buf = Buffer.allocUnsafe(n);
      let off = 0;
      while (off < n) {
        const k = readSync(fd, buf, off, n - off, pos + off);
        if (k <= 0) break;
        off += k;
      }
      return new Uint8Array(buf.buffer, buf.byteOffset, off);
    },
  };
}

const files = {};
const names = [
  "config.json",
  "generation_config.json",
  "tokenizer.json",
  "tokenizer_config.json",
  "special_tokens_map.json",
];
for (const name of names) {
  files["model/" + name] = readFileSync(join(workerData.model, name));
}
files["model/model.safetensors"] = disk(join(workerData.model, "model.safetensors"));
for (const name of readdirSync(workerData.kernels)) {
  if (name.endsWith(".wgsl")) files["kernels/" + name] = readFileSync(join(workerData.kernels, name));
}

const wasm = readFileSync(workerData.wasm);
const args = ["bitnet.wasm", "-model", "model", "-kernels", "kernels"];
if (workerData.ui) args.push("-ui");
const loaded = await instantiate(wasm, {
  args,
  files,
  stdinSab: workerData.sab,
  stdout: (t) => parentPort.postMessage({ type: "out", text: t }),
  stderr: (t) => parentPort.postMessage({ type: "err", text: t }),
  webgpu: gpu,
});
let code = 0;
try {
  code = await run(loaded);
} catch (err) {
  parentPort.postMessage({ type: "err", text: (err && err.stack) || String(err) });
  code = 1;
}
parentPort.postMessage({ type: "exit", code: code | 0 });
