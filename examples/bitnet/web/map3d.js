// The map's 3D views (flowmap.js owns the data, the 2D view and the mode switch).
// volume: one role's voxels, layer x output cell x input cell: balance.<role> at rest,
// the token's currents once frames arrive. graph: every layer's cells as nodes with the
// layers along depth, the strongest currents of each layer as edges, and the residual
// carried from each layer's D' into the next layer's D. WebGL2 with additive blending,
// so nothing is depth-sorted. Drag turns, the wheel zooms, a double click resets.

const COPPER = [232 / 255, 168 / 255, 124 / 255];
const BLUE = [126 / 255, 182 / 255, 232 / 255];
const INK = [236 / 255, 231 / 255, 223 / 255];
const FOV = Math.PI / 4;

const POINT_VS = `#version 300 es
layout(location = 0) in vec3 pos;
layout(location = 1) in vec4 col;
layout(location = 2) in float size;
uniform mat4 mvp;
uniform float px;
out vec4 vcol;
void main() {
  gl_Position = mvp * vec4(pos, 1.0);
  gl_PointSize = clamp(size * px / gl_Position.w, 1.0, 48.0);
  vcol = col;
}`;
const POINT_FS = `#version 300 es
precision mediump float;
in vec4 vcol;
out vec4 o;
void main() {
  vec2 p = gl_PointCoord * 2.0 - 1.0;
  float r = dot(p, p);
  if (r > 1.0) discard;
  float a = vcol.a * (1.0 - 0.7 * r);
  o = vec4(vcol.rgb * a, a);
}`;
const LINE_VS = `#version 300 es
layout(location = 0) in vec3 pos;
layout(location = 1) in vec4 col;
uniform mat4 mvp;
out vec4 vcol;
void main() {
  gl_Position = mvp * vec4(pos, 1.0);
  vcol = col;
}`;
const LINE_FS = `#version 300 es
precision mediump float;
in vec4 vcol;
out vec4 o;
void main() {
  o = vec4(vcol.rgb * vcol.a, vcol.a);
}`;

// src: { layers, counts {D,Q,KV,F,Dout}, layer, live, energy, matrix(l, role) -> {rows, cols, data},
//        node(l, id) -> Float32Array | null }
export function mount3D(canvas) {
  const gl = canvas.getContext("webgl2", { antialias: true, alpha: true, premultipliedAlpha: true });
  if (!gl) return null;
  const points = program(gl, POINT_VS, POINT_FS);
  const lines = program(gl, LINE_VS, LINE_FS);
  const pBuf = gl.createBuffer();
  const lBuf = gl.createBuffer();
  const pVao = gl.createVertexArray();
  const lVao = gl.createVertexArray();
  gl.bindVertexArray(pVao);
  gl.bindBuffer(gl.ARRAY_BUFFER, pBuf);
  gl.enableVertexAttribArray(0);
  gl.vertexAttribPointer(0, 3, gl.FLOAT, false, 32, 0);
  gl.enableVertexAttribArray(1);
  gl.vertexAttribPointer(1, 4, gl.FLOAT, false, 32, 12);
  gl.enableVertexAttribArray(2);
  gl.vertexAttribPointer(2, 1, gl.FLOAT, false, 32, 28);
  gl.bindVertexArray(lVao);
  gl.bindBuffer(gl.ARRAY_BUFFER, lBuf);
  gl.enableVertexAttribArray(0);
  gl.vertexAttribPointer(0, 3, gl.FLOAT, false, 28, 0);
  gl.enableVertexAttribArray(1);
  gl.vertexAttribPointer(1, 4, gl.FLOAT, false, 28, 12);
  gl.bindVertexArray(null);

  const home = { yaw: 0.62, pitch: 0.38, dist: 3.6 };
  const cam = Object.assign({}, home);
  let nPoints = 0;
  let nLines = 0;
  let queued = false;
  let onTurn = null;

  function upload(p, l) {
    gl.bindBuffer(gl.ARRAY_BUFFER, pBuf);
    gl.bufferData(gl.ARRAY_BUFFER, p.out(), gl.DYNAMIC_DRAW);
    nPoints = p.n;
    gl.bindBuffer(gl.ARRAY_BUFFER, lBuf);
    gl.bufferData(gl.ARRAY_BUFFER, l.out(), gl.DYNAMIC_DRAW);
    nLines = l.n;
  }

  function render() {
    queued = false;
    const dpr = Math.min(2, window.devicePixelRatio || 1);
    const w = Math.round(canvas.clientWidth * dpr);
    const h = Math.round(canvas.clientHeight * dpr);
    if (!w || !h) return;
    if (canvas.width !== w || canvas.height !== h) {
      canvas.width = w;
      canvas.height = h;
    }
    gl.viewport(0, 0, w, h);
    gl.clearColor(0, 0, 0, 0);
    gl.clear(gl.COLOR_BUFFER_BIT);
    gl.enable(gl.BLEND);
    gl.blendFunc(gl.ONE, gl.ONE);
    gl.disable(gl.DEPTH_TEST);
    const mvp = viewProj(cam, w / h);
    gl.useProgram(lines.p);
    gl.uniformMatrix4fv(lines.u.mvp, false, mvp);
    gl.bindVertexArray(lVao);
    gl.drawArrays(gl.LINES, 0, nLines);
    gl.useProgram(points.p);
    gl.uniformMatrix4fv(points.u.mvp, false, mvp);
    gl.uniform1f(points.u.px, h / (2 * Math.tan(FOV / 2)));
    gl.bindVertexArray(pVao);
    gl.drawArrays(gl.POINTS, 0, nPoints);
    gl.bindVertexArray(null);
  }

  function redraw() {
    if (queued) return;
    queued = true;
    requestAnimationFrame(render);
  }

  // A new picture: rebuild the buffers from src in the given mode.
  function show(mode, src, role) {
    const p = new Verts(8);
    const l = new Verts(7);
    if (mode === "graph") graph(src, p, l);
    else volume(src, role, p, l);
    upload(p, l);
    redraw();
  }

  let drag = null;
  canvas.addEventListener("pointerdown", (ev) => {
    drag = { x: ev.clientX, y: ev.clientY };
    canvas.setPointerCapture(ev.pointerId);
  });
  canvas.addEventListener("pointermove", (ev) => {
    if (!drag) return;
    cam.yaw -= (ev.clientX - drag.x) * 0.008;
    cam.pitch = Math.max(-1.45, Math.min(1.45, cam.pitch + (ev.clientY - drag.y) * 0.008));
    drag = { x: ev.clientX, y: ev.clientY };
    if (onTurn) onTurn();
    redraw();
  });
  const end = () => { drag = null; };
  canvas.addEventListener("pointerup", end);
  canvas.addEventListener("pointercancel", end);
  canvas.addEventListener("wheel", (ev) => {
    ev.preventDefault();
    cam.dist = Math.max(1.2, Math.min(12, cam.dist * Math.exp(ev.deltaY * 0.0012)));
    redraw();
  }, { passive: false });
  canvas.addEventListener("dblclick", () => {
    Object.assign(cam, home);
    redraw();
  });

  return {
    show,
    redraw,
    set onTurn(fn) { onTurn = fn; },
  };
}

// Growable interleaved float vertices.
class Verts {
  constructor(stride) {
    this.stride = stride;
    this.a = new Float32Array(stride * 4096);
    this.n = 0;
  }
  push(...v) {
    if ((this.n + 1) * this.stride > this.a.length) {
      const b = new Float32Array(this.a.length * 2);
      b.set(this.a);
      this.a = b;
    }
    this.a.set(v, this.n * this.stride);
    this.n++;
  }
  out() {
    return this.a.subarray(0, this.n * this.stride);
  }
}

function tint(v) {
  return v >= 0 ? COPPER : BLUE;
}

function box(l, x0, x1, y0, y1, z0, z1, a) {
  const c = [x0, y0, z0, x1, y0, z0, x1, y1, z0, x0, y1, z0, x0, y0, z1, x1, y0, z1, x1, y1, z1, x0, y1, z1];
  const e = [0, 1, 1, 2, 2, 3, 3, 0, 4, 5, 5, 6, 6, 7, 7, 4, 0, 4, 1, 5, 2, 6, 3, 7];
  for (const i of e) l.push(c[3 * i], c[3 * i + 1], c[3 * i + 2], INK[0], INK[1], INK[2], a);
}

// One role's voxels: x = input cell, y = layer, z = output cell. Each voxel's size and light
// is sqrt(|current| / the volume's largest), copper for positive and blue for negative.
function volume(src, role, p, l) {
  const L = src.layers;
  if (!L) return;
  const first = src.matrix(0, role);
  if (!first) return;
  const { rows, cols } = first;
  const mats = [];
  for (let k = 0; k < L; k++) mats.push(src.matrix(k, role));
  const max = high(mats);
  const dz = Math.max(0.6, Math.min(2.2, 2 * rows / cols));
  const hy = 1.6;
  const sx = 2 / cols, sz = dz / rows, sy = hy / L;
  const cell = Math.min(sx, sz, sy) * 1.15;
  for (let k = 0; k < L; k++) {
    const m = mats[k];
    if (!m) continue;
    const y = -hy / 2 + (k + 0.5) * sy;
    const lit = k === src.layer ? 1 : 0.55;
    for (let r = 0; r < rows; r++) {
      const z = -dz / 2 + (r + 0.5) * sz;
      for (let c = 0; c < cols; c++) {
        const v = m.data[r * cols + c];
        const t = Math.min(1, Math.abs(v) / max);
        if (t < 0.3) continue;
        const col = tint(v);
        p.push(-1 + (c + 0.5) * sx, y, z, col[0], col[1], col[2], lit * (0.02 + 0.3 * t * t), cell * (0.4 + 0.7 * t));
      }
    }
  }
  box(l, -1, 1, -hy / 2, hy / 2, -dz / 2, dz / 2, 0.16);
  const y = -hy / 2 + (src.layer + 0.5) * sy;
  for (const [a, b] of [[[-1, -dz / 2], [1, -dz / 2]], [[1, -dz / 2], [1, dz / 2]], [[1, dz / 2], [-1, dz / 2]], [[-1, dz / 2], [-1, -dz / 2]]]) {
    l.push(a[0], y, a[1], COPPER[0], COPPER[1], COPPER[2], 0.9);
    l.push(b[0], y, b[1], COPPER[0], COPPER[1], COPPER[2], 0.9);
  }
}

// The 99.5th percentile of |value| over the matrices (one hot voxel does not dim the rest), from a
// sample of at most 40,000 values.
function high(mats) {
  let n = 0;
  for (const m of mats) if (m) n += m.data.length;
  const step = Math.max(1, Math.floor(n / 40000));
  const s = [];
  let i = 0;
  for (const m of mats) {
    if (!m) continue;
    for (let j = 0; j < m.data.length; j++, i++) if (i % step === 0) s.push(Math.abs(m.data[j]));
  }
  if (!s.length) return 1e-30;
  s.sort((a, b) => a - b);
  return Math.max(1e-30, s[Math.min(s.length - 1, Math.floor(s.length * 0.995))]);
}

// The roles as the 2D map draws them: source axis a (columns), destination axis b (rows).
const EDGES = [
  { role: 0, a: "D", b: "Q" }, { role: 1, a: "D", b: "KV" }, { role: 2, a: "D", b: "KV" },
  { role: 3, a: "Q", b: "Dout" }, { role: 4, a: "D", b: "F" }, { role: 5, a: "D", b: "F" },
  { role: 6, a: "F", b: "Dout" },
];
const NODE = { D: 0, Dout: 2, F: 4, Q: 5, KV: 6 };
const COLX = { D: -1, Q: -0.25, KV: -0.25, F: 0.45, Dout: 1 };
const PER_ROLE = 18;

// Every layer is one slice of the 2D map, the layers along z.
function graph(src, p, l) {
  const L = src.layers;
  if (!L) return;
  const span = 2.6;
  const zOf = (k) => (L === 1 ? 0 : -span / 2 + span * k / (L - 1));
  const band = (name) => name === "Q" ? [0.8, -0.13] : name === "KV" ? [-0.32, -0.8] : [0.8, -0.8];
  const pos = (name, i, k) => {
    const n = src.counts[name];
    const [y0, y1] = band(name);
    const t = n === 1 ? 0.5 : i / (n - 1);
    return [COLX[name], y0 + t * (y1 - y0), zOf(k)];
  };
  const maxE = Math.max(1e-30, ...(src.energy || [1]));
  const lit = (k) => k === src.layer ? 1 : 0.06 + 0.3 * ((src.energy && src.energy[k]) || 0) / maxE;

  let edgeMax = 1e-30;
  const picked = [];
  for (let k = 0; k < L; k++) {
    for (const e of EDGES) {
      const m = src.matrix(k, e.role);
      if (!m) continue;
      const take = strongest(m, PER_ROLE);
      for (const t of take) edgeMax = Math.max(edgeMax, t.a);
      picked.push([k, e, take]);
    }
  }
  for (const [k, e, take] of picked) {
    const a = lit(k);
    for (const t of take) {
      const from = pos(e.a, t.c, k);
      const to = pos(e.b, t.r, k);
      const mag = t.a / edgeMax;
      const col = tint(t.v);
      const al = a * (0.02 + 0.5 * mag * mag);
      l.push(from[0], from[1], from[2], col[0], col[1], col[2], al);
      l.push(to[0], to[1], to[2], col[0], col[1], col[2], al);
    }
  }
  // The residual: layer k's D' is layer k+1's D.
  for (let k = 0; k + 1 < L; k++) {
    for (let i = 0; i < src.counts.D; i += 4) {
      const a = pos("Dout", i, k);
      const b = pos("D", i, k + 1);
      l.push(a[0], a[1], a[2], INK[0], INK[1], INK[2], 0.025);
      l.push(b[0], b[1], b[2], INK[0], INK[1], INK[2], 0.025);
    }
  }
  for (let k = 0; k < L; k++) {
    const a = lit(k);
    for (const name of ["D", "Q", "KV", "F", "Dout"]) {
      const n = src.counts[name];
      const vals = src.live ? src.node(k, NODE[name]) : null;
      let max = 1e-30;
      if (vals) for (let i = 0; i < n; i++) max = Math.max(max, Math.abs(vals[i] || 0));
      for (let i = 0; i < n; i++) {
        const q = pos(name, i, k);
        if (!vals) {
          p.push(q[0], q[1], q[2], INK[0], INK[1], INK[2], 0.3 * a, 0.016);
          continue;
        }
        const v = vals[i] || 0;
        const t = Math.abs(v) / max;
        const col = tint(v);
        p.push(q[0], q[1], q[2], col[0], col[1], col[2], a * (0.15 + 0.7 * t), 0.018 + 0.03 * t);
      }
    }
  }
}

// The n largest |values| of a matrix. Rows are destination cells, columns are sources.
function strongest(m, n) {
  const heap = [];
  let min = 0;
  for (let i = 0; i < m.data.length; i++) {
    const v = m.data[i];
    const a = Math.abs(v);
    if (a === 0) continue;
    if (heap.length < n) {
      heap.push({ v, a, r: (i / m.cols) | 0, c: i % m.cols });
      if (heap.length === n) min = lowest(heap);
      continue;
    }
    if (a <= heap[min].a) continue;
    heap[min] = { v, a, r: (i / m.cols) | 0, c: i % m.cols };
    min = lowest(heap);
  }
  return heap;
}

function lowest(heap) {
  let at = 0;
  for (let k = 1; k < heap.length; k++) if (heap[k].a < heap[at].a) at = k;
  return at;
}

function program(gl, vs, fs) {
  const p = gl.createProgram();
  for (const [type, text] of [[gl.VERTEX_SHADER, vs], [gl.FRAGMENT_SHADER, fs]]) {
    const s = gl.createShader(type);
    gl.shaderSource(s, text);
    gl.compileShader(s);
    if (!gl.getShaderParameter(s, gl.COMPILE_STATUS)) throw new Error("map3d: " + gl.getShaderInfoLog(s));
    gl.attachShader(p, s);
  }
  gl.linkProgram(p);
  if (!gl.getProgramParameter(p, gl.LINK_STATUS)) throw new Error("map3d: " + gl.getProgramInfoLog(p));
  return { p, u: { mvp: gl.getUniformLocation(p, "mvp"), px: gl.getUniformLocation(p, "px") } };
}

// Perspective times look-at, column-major, the camera orbiting the origin.
function viewProj(cam, aspect) {
  // A tall panel backs the camera off so the picture fits its width.
  const dist = cam.dist * Math.max(1, 1 / aspect);
  const cp = Math.cos(cam.pitch);
  const eye = [dist * cp * Math.sin(cam.yaw), dist * Math.sin(cam.pitch), dist * cp * Math.cos(cam.yaw)];
  const f = norm([-eye[0], -eye[1], -eye[2]]);
  const s = norm(cross(f, [0, 1, 0]));
  const u = cross(s, f);
  const view = [
    s[0], u[0], -f[0], 0,
    s[1], u[1], -f[1], 0,
    s[2], u[2], -f[2], 0,
    -dot(s, eye), -dot(u, eye), dot(f, eye), 1,
  ];
  const near = 0.05, far = 50;
  const t = 1 / Math.tan(FOV / 2);
  const proj = [
    t / aspect, 0, 0, 0,
    0, t, 0, 0,
    0, 0, (far + near) / (near - far), -1,
    0, 0, (2 * far * near) / (near - far), 0,
  ];
  return new Float32Array(mul(proj, view));
}

function mul(a, b) {
  const o = new Array(16).fill(0);
  for (let c = 0; c < 4; c++) for (let r = 0; r < 4; r++) for (let k = 0; k < 4; k++) o[c * 4 + r] += a[k * 4 + r] * b[c * 4 + k];
  return o;
}
function cross(a, b) { return [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]; }
function dot(a, b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
function norm(a) { const n = Math.hypot(a[0], a[1], a[2]) || 1; return [a[0] / n, a[1] / n, a[2] / n]; }
