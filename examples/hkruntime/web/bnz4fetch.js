// Fills the weight heap with a BNZ4 model's decoded bytes (bnz4load.js), on this worker's own GPU
// device. The guest reads the heap while later segments are still arriving.
import { fillHeap4 } from "./bnz4load.js";

self.onmessage = async (e) => {
  const m = e.data || {};
  try {
    await fillHeap4({ url: m.url, prefix: m.prefix, step: m.step, window: m.window || null }, { buffer: m.buf }, m.table, new Int32Array(m.flags), (p) => {
      self.postMessage({ type: "progress", p });
    });
    self.postMessage({ type: "done" });
  } catch (err) {
    self.postMessage({ type: "error", text: err && err.message ? err.message : String(err) });
  }
};
