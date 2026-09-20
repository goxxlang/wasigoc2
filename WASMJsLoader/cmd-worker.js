// Worker occupancy hop: instantiate cmdterm.wasm (wasigocvm) with
// xterm stdin on a SharedArrayBuffer. No host bridge — GocOS/Win32
// are inside the module.
import { instantiate, run } from "./loader.js";

self.onmessage = async (e) => {
  const msg = e.data;
  if (!msg || msg.type !== "boot") return;
  try {
    const inst = await instantiate(msg.bytes, {
      args: ["cmdterm.wasm"],
      stdout: (t) => self.postMessage({ type: "out", text: t }),
      stderr: (t) => self.postMessage({ type: "err", text: t }),
      stdinSab: msg.sab,
    });
    const code = await run(inst);
    self.postMessage({ type: "exit", code });
  } catch (err) {
    self.postMessage({
      type: "err",
      text: err && err.message ? err.message : String(err),
    });
  }
};
