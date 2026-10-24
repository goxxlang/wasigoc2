// The network map. volume.safetensors is the cochain: balance.* is the resting
// current between cells, and each live frame (flow.js) replaces that with the
// token's activations. Columns are the axes, lines are the strongest cell-to-cell
// currents, and the rail is one tick per layer. The same data is drawn three ways:
// 2D (one layer), 3D volume (one role's voxels over every layer) and 3D graph
// (every layer's slice, along depth); map3d.js draws the 3D ones.
import { mount3D } from "./map3d.js";

const MODES = ["2d", "volume", "graph"];
const ROLE_NAMES = ["q", "k", "v", "o", "gate", "up", "down"];

function saved(key, fallback, ok) {
  try {
    const v = localStorage.getItem(key);
    return v && ok(v) ? v : fallback;
  } catch {
    return fallback;
  }
}

function keep(key, value) {
  try { localStorage.setItem(key, value); } catch { /* storage off */ }
}

const EDGES = [
  { role: 0, name: "q", a: "D", b: "Q", vol: "balance.q" },
  { role: 1, name: "k", a: "D", b: "KV", vol: "balance.k" },
  { role: 2, name: "v", a: "D", b: "KV", vol: "balance.v" },
  { role: 3, name: "o", a: "Q", b: "Dout", vol: "balance.o" },
  { role: 4, name: "gate", a: "D", b: "F", vol: "balance.gate" },
  { role: 5, name: "up", a: "D", b: "F", vol: "balance.up" },
  { role: 6, name: "down", a: "F", b: "Dout", vol: "balance.down" },
];

const NODE = { D: 0, Dout: 2, F: 4, Q: 5, KV: 6 };

export function mountMap(root) {
  const canvas = root.querySelector("#map-canvas");
  const glCanvas = root.querySelector("#map-gl");
  const meta = root.querySelector("#map-meta");
  const followBtn = root.querySelector("#map-follow");
  const roleSel = root.querySelector("#map-role");
  const modeBtns = [...root.querySelectorAll("[data-map-mode]")];
  const ctx = canvas.getContext("2d");
  let gl3d = null;
  try { gl3d = mount3D(glCanvas); } catch (err) { console.error(err); gl3d = null; }
  let mode = saved("champ.map", "2d", (v) => MODES.includes(v));
  let role = Number(saved("champ.map.role", "0", (v) => /^[0-6]$/.test(v)));
  if (!gl3d) {
    mode = "2d";
    for (const b of modeBtns) {
      if (b.dataset.mapMode === "2d") continue;
      b.disabled = true;
      b.title = "3D needs WebGL2, which this browser does not have.";
    }
  }
  let vol = null;
  let counts = null;
  let layers = 0;
  let layer = 0;
  let follow = true;
  let frame = null;
  let token = "";
  let energy = null;
  let line = "";

  function say(text) {
    line = text;
    const how = mode === "volume" ? " · " + ROLE_NAMES[role] + " voxels" : mode === "graph" ? " · all layers" : "";
    meta.textContent = text + how;
  }

  function setMode(m) {
    mode = MODES.includes(m) ? m : "2d";
    keep("champ.map", mode);
    for (const b of modeBtns) b.setAttribute("aria-pressed", b.dataset.mapMode === mode ? "true" : "false");
    canvas.hidden = mode !== "2d";
    glCanvas.hidden = mode === "2d";
    roleSel.hidden = mode !== "volume";
    say(line);
    draw();
  }

  function structure(bytes) {
    vol = parseSafetensors(bytes);
    const q = vol.get("balance.q");
    const k = vol.get("balance.k");
    const g = vol.get("balance.gate");
    if (!q || !k || !g) {
      say("This volume has no cell map.");
      return;
    }
    layers = q.shape[0];
    counts = { D: q.shape[2], Q: q.shape[1], KV: k.shape[1], F: g.shape[1], Dout: q.shape[2] };
    energy = new Float32Array(layers);
    layer = hottest();
    say(counts.D + " D · " + counts.Q + " Q · " + counts.KV + " KV · " + counts.F + " F");
    draw();
    requestAnimationFrame(() => draw());
  }

  function note(text) {
    token = text || "";
  }

  function show(fr) {
    frame = fr;
    if (!counts && fr.layout) countsFromLayout(fr.layout);
    if (follow) layer = hottest();
    const cons = fr.mismatch < 1e-3 ? "conserved" : fr.mismatch.toExponential(2);
    const word = token ? " · " + JSON.stringify(token) : "";
    say("L" + layer + " · pos " + fr.pos + word + " · " + cons);
    draw();
  }

  function countsFromLayout(L) {
    layers = L.layers;
    counts = {
      D: L.axes.D.cells, Q: L.axes.Q.cells, KV: L.axes.KV.cells, F: L.axes.F.cells,
      Dout: L.axes.D.cells,
    };
    if (!energy || energy.length !== layers) energy = new Float32Array(layers);
  }

  function hottest() {
    let best = 0, at = 0;
    for (let i = 0; i < layers; i++) {
      const e = layerEnergy(i);
      energy[i] = e;
      if (e > best) { best = e; at = i; }
    }
    return at;
  }

  function layerEnergy(l) {
    let s = 0;
    for (const e of EDGES) {
      const m = matrix(l, e);
      if (!m) continue;
      for (let i = 0; i < m.data.length; i++) s += Math.abs(m.data[i]);
    }
    return s;
  }

  function matrix(l, edge) {
    if (frame) {
      const L = frame.layout;
      const rows = L.axes[L.roles[edge.role].rows].cells;
      const cols = L.axes[L.roles[edge.role].cols].cells;
      const off = L.edge[l * 7 + edge.role];
      return { rows, cols, data: new Float32Array(frame.flow.buffer, frame.flow.byteOffset + off, rows * cols) };
    }
    const t = vol && vol.get(edge.vol);
    if (!t) return null;
    const [, rows, cols] = t.shape;
    const n = rows * cols;
    return { rows, cols, data: t.data.subarray(l * n, (l + 1) * n) };
  }

  function nodeValue(name, i) {
    if (!frame) return 0;
    const v = nodeVec(NODE[name]);
    return v ? v[i] || 0 : 0;
  }

  function nodeVec(id) {
    return nodeVecAt(layer, id);
  }

  function nodeVecAt(l, id) {
    const L = frame.layout;
    const n = L.axes[L.nodes[id].axis].cells;
    const off = L.node[l * 10 + id];
    return new Float32Array(frame.flow.buffer, frame.flow.byteOffset + off, n);
  }

  function draw() {
    if (!counts) return;
    if (mode === "2d") {
      draw2d();
      return;
    }
    gl3d.show(mode, {
      layers, counts, layer, energy,
      live: !!frame,
      matrix: (l, r) => matrix(l, EDGES[r]),
      node: (l, id) => (frame ? nodeVecAt(l, id) : null),
    }, role);
  }

  function draw2d() {
    const cssW = root.clientWidth;
    const cssH = canvas.clientHeight || root.clientHeight - 36;
    if (!cssW || !cssH || !counts) return;
    const dpr = Math.min(2, window.devicePixelRatio || 1);
    canvas.width = Math.round(cssW * dpr);
    canvas.height = Math.round(cssH * dpr);
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    ctx.clearRect(0, 0, cssW, cssH);

    const rail = 18;
    const top = 22;
    const bot = cssH - 16;
    const span = Math.max(1, bot - top);
    const xOf = (name) => {
      const f = { D: 0.14, Q: 0.40, KV: 0.40, F: 0.68, Dout: 0.90 }[name];
      return rail + 8 + f * (cssW - rail - 16);
    };
    const band = (name) => name === "Q" ? [top, top + span * 0.58] : name === "KV" ? [top + span * 0.70, bot] : [top, bot];
    const cols = {};
    for (const name of ["D", "Q", "KV", "F", "Dout"]) {
      const [y0, y1] = band(name);
      const n = counts[name];
      cols[name] = [];
      for (let i = 0; i < n; i++) {
        const t = n === 1 ? 0.5 : i / (n - 1);
        cols[name].push({ x: xOf(name), y: y0 + t * (y1 - y0) });
      }
    }

    const maxE = energy ? Math.max(...energy, 1e-12) : 1;
    for (let i = 0; i < layers; i++) {
      const y0 = (i / layers) * cssH;
      const h = cssH / layers;
      const hot = energy ? energy[i] / maxE : 0;
      ctx.fillStyle = i === layer ? "rgba(232,168,124,0.95)" : "rgba(232,168,124," + (0.15 + 0.7 * hot) + ")";
      ctx.fillRect(4, y0 + 1, 6, Math.max(2, h - 2));
    }

    ctx.font = "11px Segoe UI, system-ui, sans-serif";
    ctx.fillStyle = "#8e96a3";
    ctx.textAlign = "center";
    ctx.fillText("D", xOf("D"), 14);
    ctx.fillText("Q", xOf("Q"), 14);
    ctx.fillText("F", xOf("F"), 14);
    ctx.fillText("D′", xOf("Dout"), 14);
    const kv = band("KV");
    ctx.fillText("KV", xOf("KV"), kv[0] - 4);

    const picked = [];
    let edgeMax = 1e-12;
    for (const e of EDGES) {
      const m = matrix(layer, e);
      if (!m) continue;
      const take = strongest(m, 70);
      for (const p of take) edgeMax = Math.max(edgeMax, Math.abs(p.v));
      picked.push([e, m, take]);
    }
    for (const [e, , take] of picked) {
      for (const p of take) {
        const from = cols[e.a][p.c];
        const to = cols[e.b][p.r];
        if (!from || !to) continue;
        const mag = Math.abs(p.v) / edgeMax;
        ctx.strokeStyle = p.v >= 0 ? "rgba(232,168,124," + (0.15 + 0.8 * mag) + ")" : "rgba(126,182,232," + (0.15 + 0.8 * mag) + ")";
        ctx.lineWidth = 0.6 + 1.6 * mag;
        ctx.beginPath();
        ctx.moveTo(from.x, from.y);
        ctx.lineTo(to.x, to.y);
        ctx.stroke();
      }
    }

    for (const name of ["D", "Q", "KV", "F", "Dout"]) {
      let max = 1e-12;
      const vals = cols[name].map((_, i) => {
        const v = nodeValue(name, i);
        max = Math.max(max, Math.abs(v));
        return v;
      });
      cols[name].forEach((p, i) => {
        const v = vals[i];
        const mag = Math.abs(v) / max;
        const r = frame ? 1.6 + 3.2 * mag : 1.8;
        ctx.fillStyle = !frame ? "rgba(236,231,223,0.85)" : v >= 0 ? "rgba(232,168,124," + (0.45 + 0.55 * mag) + ")" : "rgba(126,182,232," + (0.45 + 0.55 * mag) + ")";
        ctx.beginPath();
        ctx.arc(p.x, p.y, r, 0, Math.PI * 2);
        ctx.fill();
      });
    }
  }

  canvas.addEventListener("click", (ev) => {
    const rect = canvas.getBoundingClientRect();
    const x = ev.clientX - rect.left;
    const y = ev.clientY - rect.top;
    if (x > 22 || !layers) return;
    layer = Math.min(layers - 1, Math.max(0, Math.floor(y / rect.height * layers)));
    follow = false;
    followBtn.setAttribute("aria-pressed", "false");
    if (frame) say("L" + layer + " · pinned");
    draw();
  });
  followBtn.addEventListener("click", () => {
    follow = !follow;
    followBtn.setAttribute("aria-pressed", follow ? "true" : "false");
    if (follow && frame) layer = hottest();
    draw();
  });
  for (const b of modeBtns) b.addEventListener("click", () => setMode(b.dataset.mapMode));
  roleSel.value = String(role);
  roleSel.addEventListener("change", () => {
    role = Number(roleSel.value) || 0;
    keep("champ.map.role", String(role));
    say(line);
    draw();
  });
  // In 3D the arrow keys pick the layer, as the rail does in 2D.
  glCanvas.addEventListener("keydown", (ev) => {
    if (!layers || (ev.key !== "ArrowUp" && ev.key !== "ArrowDown")) return;
    ev.preventDefault();
    layer = Math.min(layers - 1, Math.max(0, layer + (ev.key === "ArrowUp" ? 1 : -1)));
    follow = false;
    followBtn.setAttribute("aria-pressed", "false");
    say("L" + layer + " · pinned");
    draw();
  });
  const ro = new ResizeObserver(() => (mode === "2d" ? draw() : gl3d.redraw()));
  ro.observe(root);
  setMode(mode);

  return { structure, frame: show, token: note };
}

// The n largest |currents|. Rows are destination cells, columns are sources.
function strongest(m, n) {
  const heap = [];
  for (let i = 0; i < m.data.length; i++) {
    const v = m.data[i];
    const a = Math.abs(v);
    if (a === 0) continue;
    const item = { v, a, r: (i / m.cols) | 0, c: i % m.cols };
    if (heap.length < n) {
      heap.push(item);
      continue;
    }
    let min = 0;
    for (let k = 1; k < heap.length; k++) if (heap[k].a < heap[min].a) min = k;
    if (a > heap[min].a) heap[min] = item;
  }
  return heap;
}

function parseSafetensors(buf) {
  const dv = new DataView(buf.buffer, buf.byteOffset, buf.byteLength);
  const n = Number(dv.getBigUint64(0, true));
  const header = JSON.parse(new TextDecoder().decode(buf.subarray(8, 8 + n)));
  const base = 8 + n;
  const map = new Map();
  for (const [name, meta] of Object.entries(header)) {
    if (!meta || !meta.shape || !meta.data_offsets) continue;
    const [a, b] = meta.data_offsets;
    const off = buf.byteOffset + base + a;
    const count = (b - a) / 4;
    map.set(name, {
      shape: meta.shape,
      data: meta.dtype === "F32" ? new Float32Array(buf.buffer, off, count) : null,
    });
  }
  return map;
}
