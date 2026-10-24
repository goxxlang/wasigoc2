// Champ's flow instrumentation, page side (model/flow.go). With ?flow, the model folder's
// volume.safetensors gives the cells, the worker gets two shared-memory files, and every generated
// token's currents arrive as a frame: "flow" holds a 16-byte header (sequence, position, token id,
// flow bytes; little-endian u32) and the flow, laid out as the ready event's flow layout says.
// Each frame is copied out, acknowledged by writing its sequence into "flow.ack" (the model waits for
// that before the next one), checked, and handed to whoever listens.

export const FLOW_BYTES = 16 << 20;

export function createFlow(enabled) {
  const on = !!enabled;
  const sab = on ? new SharedArrayBuffer(FLOW_BYTES) : null;
  const ackSab = on ? new SharedArrayBuffer(4) : null;
  const listeners = [];
  let layout = null;

  // A frame's checks, as hodgemix's (cochain.go conservation): the residual is conserved at every
  // node (carried value plus writes = the node's value), and every BitLinear output cell's currents
  // sum to its cell sum. Mismatches are relative to the summed magnitudes flowing in.
  function check(f) {
    const L = layout.layers;
    const ax = layout.axes;
    const role = (l, r) => new Float32Array(f.buffer, f.byteOffset + layout.edge[l * 7 + r], cellsOf(layout.roles[r].rows) * cellsOf(layout.roles[r].cols));
    const node = (l, n) => new Float32Array(f.buffer, f.byteOffset + layout.node[l * 10 + n], cellsOf(layout.nodes[n].axis));
    function cellsOf(a) { return ax[a].cells; }
    let worst = 0;
    const chk = (inn, abs, val) => { worst = Math.max(worst, Math.abs(inn - val) / Math.max(abs, 1e-30)); };
    function rowsum(e, nc, r) {
      let s = 0, a = 0;
      for (let c = 0; c < nc; c++) { const x = e[r * nc + c]; s += x; a += Math.abs(x); }
      return [s, a];
    }
    const ND = ax.D.cells, NQ = ax.Q.cells, NF = ax.F.cells, NKV = ax.KV.cells;
    for (let l = 0; l < L; l++) {
      const D = node(l, 0), Dm = node(l, 1), Dout = node(l, 2);
      const o = role(l, 3), down = role(l, 6);
      for (let i = 0; i < ND; i++) {
        let [s, a] = rowsum(o, NQ, i);
        chk(D[i] + s, Math.abs(D[i]) + a, Dm[i]);
        [s, a] = rowsum(down, NF, i);
        chk(Dm[i] + s, Math.abs(Dm[i]) + a, Dout[i]);
      }
      const outs = [[0, 5, NQ, ND], [1, 6, NKV, ND], [2, 7, NKV, ND], [4, 8, NF, ND], [5, 9, NF, ND]]; // role, node
      for (const [r, n, nr, nc] of outs) {
        const e = role(l, r), v = node(l, n);
        for (let i = 0; i < nr; i++) { const [s, a] = rowsum(e, nc, i); chk(s, a, v[i]); }
      }
    }
    return worst;
  }

  return {
    on,
    // files and args for the worker
    wire(files, args) {
      if (!on) return;
      files.flow = { sab, off: 0, size: FLOW_BYTES };
      files["flow.ack"] = { sab: ackSab, off: 0, size: 4 };
      args.push("-flow", "flow");
    },
    ready(l) {
      if (!on) return;
      if (!l || !l.bytes) throw new Error("Hodge-3.5 started without its flow instrumentation.");
      if (16 + l.bytes > FLOW_BYTES) throw new Error("A flow frame of " + l.bytes + " bytes does not fit the shared memory.");
      layout = l;
    },
    // ev: {"ev":"flow","seq","pos","id"}
    frame(ev) {
      if (!on || !layout) return;
      const head = new DataView(sab, 0, 16);
      const seq = head.getUint32(0, true), pos = head.getUint32(4, true), id = head.getUint32(8, true), n = head.getUint32(12, true);
      if (seq !== ev.seq || n !== layout.bytes) {
        throw new Error("flow frame " + ev.seq + ": the shared memory holds frame " + seq + " (" + n + " bytes)");
      }
      const data = new Uint8Array(n);
      data.set(new Uint8Array(sab, 16, n));
      new DataView(ackSab).setUint32(0, seq, true);
      const f = new Float32Array(data.buffer);
      const mismatch = check(f);
      const fr = { seq, pos, id, flow: f, layout, mismatch };
      for (const fn of listeners) fn(fr);
      return fr;
    },
    listen(fn) { listeners.push(fn); },
    get layout() { return layout; },
  };
}
