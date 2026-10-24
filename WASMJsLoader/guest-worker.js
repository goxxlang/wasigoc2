// Worker hop for a wasigocvm guest that may use WebGPU (import "webgpu"):
// loader.js instantiates it with WASMWebGPU's executor on this worker's own
// navigator.gpu (served at /webgpu/ by serve.mjs). No host bridge.
//
// boot message:
//   wasm   URL of the guest, or bytes   the guest
//   args   argv (args[0] is the program name)
//   env    ["K=V", ...]
//   files  {path: Uint8Array | Blob/File | {url, size} | {sab, off, size}} seeded goclibc files;
//          a Blob or {url, size} is read lazily, a piece at a time
//   sab    stdin ring (SharedArrayBuffer), optional
// replies: {type: "out" | "err", text}, {type: "exit", code}
import { instantiate, run } from "./loader.js";
import * as gpu from "/webgpu/webgpu-exec.js";

self.onmessage = async (e) => {
  const msg = e.data;
  if (msg && msg.type === "vgpu-canvas") {
    gpu.canvasAnswer(msg);
    return;
  }
  if (!msg || msg.type !== "boot") return;
  try {
    const bytes = msg.bytes || (await (await fetch(msg.wasm, { cache: "no-store" })).arrayBuffer());
    const inst = await instantiate(bytes, {
      args: msg.args || ["guest.wasm"],
      env: msg.env || [],
      files: msg.files || {},
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
