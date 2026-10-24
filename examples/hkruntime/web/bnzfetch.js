// Fills the wasm heap with BNZ chunks. The guest reads that memory while
// later chunks are still arriving. The file itself is not a JS buffer.
import { fillHeap } from "./bnzshare.js";

self.onmessage = async (e) => {
  const m = e.data || {};
  try {
    await fillHeap({
      url: m.url,
      size: m.size,
      step: m.step,
      etag: m.etag || "",
      room: m.room || "",
    }, { buffer: m.buf }, m.table, new Int32Array(m.flags), (p) => {
      self.postMessage({ type: "progress", p });
    });
    self.postMessage({ type: "done" });
  } catch (err) {
    self.postMessage({ type: "error", text: err && err.message ? err.message : String(err) });
  }
};
