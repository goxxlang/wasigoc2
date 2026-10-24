// Encryptid iEDA on HeapVM. The synthesizer is `gpu fabric eda`. The
// hypervisor is the one WSL already boots. This file only types those
// commands and shows what they print.

import { answerCanvas } from "/webgpu-page.js";
import { Plot, Prims, C, rgba } from "/lab-gfx.js";

const TypeStdin = 0x02;
const enc = new TextEncoder();

const formats = {
  verilog: ".v",
  vhdl: ".vhd",
  blif: ".blif",
  tb: "_tb.v",
  vtb: "_tb.vhd",
  rom: ".rom.txt",
  qasm: ".qasm",
  all: "",
};

class Engine {
  constructor(onStatus) {
    this.onStatus = onStatus;
    this.sab = new SharedArrayBuffer(1 << 16);
    this.i32 = new Int32Array(this.sab);
    this.u8 = new Uint8Array(this.sab);
    this.tail = "";
    this.prompts = 0;
    this.chain = Promise.resolve();
    this.ready = new Promise((resolve, reject) => {
      this.markReady = resolve;
      this.markFailed = reject;
    });
  }

  async boot() {
    if (typeof SharedArrayBuffer !== "function") {
      throw new Error("SharedArrayBuffer missing: serve with COOP/COEP (heapvm/serve.py)");
    }
    const workerReady = new Promise((resolve) => (this.markWorker = resolve));
    this.worker = new Worker("/shell-worker.js", { type: "module" });
    this.worker.onmessage = (e) => this.message(e.data || {});
    this.worker.onerror = (e) => this.markFailed(new Error(e.message || "worker error"));
    this.onStatus("fetching heap.wasm", "busy");
    const bytes = await (await fetch("/heap.wasm", { cache: "no-store" })).arrayBuffer();
    await workerReady;
    this.onStatus("booting Encryptid MicroVM (" + (bytes.byteLength / 1048576).toFixed(1) + " MB)", "busy");
    this.worker.postMessage({ type: "boot", bytes, sab: this.sab, share: null, net: null });
    return this.ready;
  }

  message(msg) {
    if (msg.type === "worker-ready") return this.markWorker();
    if (msg.type === "vgpu-canvas") return answerCanvas(msg, this.worker);
    if (msg.type === "download") return saveDownload(msg);
    if (msg.type === "out" || msg.type === "err") {
      this.tail = (this.tail + msg.text).slice(-200000);
      if (this.prompts < 2 && /[A-Z]:\\>\s*$/.test(this.tail.replace(/\x1b\[[0-9;?]*[A-Za-z]/g, ""))) {
        this.prompts++;
        this.tail = "";
        if (this.prompts === 1) setTimeout(() => this.send("\r"), 50);
        else this.markReady();
      }
      if (msg.type === "err") this.onStatus(msg.text.trim(), "fail");
    }
    if (msg.type === "exit") this.markFailed(new Error("the engine exited"));
  }

  push(bytes) {
    const cap = this.u8.length - 16;
    const w = Atomics.load(this.i32, 1);
    const r = Atomics.load(this.i32, 2);
    if (((w - r + cap) % cap) + bytes.length >= cap) throw new Error("engine stdin full");
    for (let i = 0; i < bytes.length; i++) this.u8[16 + ((w + i) % cap)] = bytes[i];
    Atomics.store(this.i32, 1, (w + bytes.length) % cap);
    Atomics.add(this.i32, 0, 1);
    Atomics.notify(this.i32, 0);
  }

  send(line) {
    const payload = enc.encode(line);
    const frame = new Uint8Array(4 + payload.length);
    frame[0] = TypeStdin;
    frame[2] = (payload.length >> 8) & 255;
    frame[3] = payload.length & 255;
    frame.set(payload, 4);
    this.push(frame);
  }

  shell(line, ms) {
    const limit = ms || 60000;
    const go = () =>
      new Promise((resolve, reject) => {
        let poll;
        const timer = setTimeout(() => {
          clearTimeout(poll);
          const sofar = this.tail.replace(/\x1b\[[0-9;?]*[A-Za-z]/g, "").replace(/\r/g, "").slice(-4000);
          reject(new Error("no prompt in " + Math.round(limit / 1000) + " s for: " + line + (sofar ? "\n" + sofar : "")));
        }, limit);
        this.tail = "";
        this.send(line + "\r");
        const look = () => {
          const clean = this.tail.replace(/\x1b\[[0-9;?]*[A-Za-z]/g, "").replace(/\r/g, "");
          if (/[A-Z]:\\>\s*$/.test(clean)) {
            clearTimeout(timer);
            resolve(clean.replace(/\s*$/, ""));
            return;
          }
          poll = setTimeout(look, 40);
        };
        poll = setTimeout(look, 40);
      });
    const p = this.chain.then(go, go);
    this.chain = p.catch(() => {});
    return p;
  }
}

function saveDownload(msg) {
  const blob = new Blob([msg.bytes], { type: "application/octet-stream" });
  const url = URL.createObjectURL(blob);
  const a = document.createElement("a");
  a.href = url;
  a.download = msg.name || "download";
  document.body.appendChild(a);
  a.click();
  a.remove();
  setTimeout(() => URL.revokeObjectURL(url), 60000);
}

const $ = (id) => document.getElementById(id);

function setStatus(text, kind) {
  const el = $("status");
  el.textContent = text;
  el.className = "status" + (kind ? " " + kind : "");
}

function setBusy(on) {
  for (const id of ["hv-run", "wsl-status", "wsl-list", "wsl-boot", "eda-refresh", "emit", "check", "download", "import", "time-load"]) {
    $(id).disabled = on;
  }
}

let engine;
let designs = [];
let selected = "";

const views = ["vm", "eda", "si", "time"];

function show(name) {
  for (const v of views) {
    $("view-" + v).classList.toggle("on", v === name);
    $("nav-" + v).classList.toggle("on", v === name);
  }
  if (name === "si") setTimeout(drawSilicon, 0);
  if (name === "time") setTimeout(showTiming, 0);
}

function parseList(text) {
  const out = [];
  for (const line of text.split("\n")) {
    const m = line.match(/^\s+(\S+)\s+(\d+)\s+LUT\s+(\d+)\s+PE\s+(\d+)\s+reg\s+(\d+)\s+rom\s+(.*)$/);
    if (!m) continue;
    if (!/^[A-Za-z_][A-Za-z0-9_]*$/.test(m[1])) continue;
    let rest = m[6];
    let blif = false;
    let qasm = false;
    if (rest.startsWith("blif")) {
      blif = true;
      rest = rest.slice(4);
    }
    rest = rest.trimStart();
    if (rest.startsWith("qasm")) {
      qasm = true;
      rest = rest.slice(4);
    }
    out.push({
      name: m[1],
      lut: m[2],
      pe: m[3],
      reg: m[4],
      rom: m[5],
      blif,
      qasm,
      what: rest.trim(),
    });
  }
  return out;
}

function paintDesigns() {
  const box = $("designs");
  box.textContent = "";
  if (!designs.length) {
    const empty = document.createElement("div");
    empty.className = "libh";
    empty.textContent = "The engine returned no designs.";
    box.appendChild(empty);
    return;
  }
  for (const d of designs) {
    const b = document.createElement("button");
    b.type = "button";
    b.className = "design" + (d.name === selected ? " on" : "");
    const name = document.createElement("span");
    name.className = "dname";
    name.textContent = d.name;
    const meta = document.createElement("span");
    meta.className = "dmeta";
    meta.textContent = d.lut + " LUT  " + d.pe + " PE  " + d.what;
    b.append(name, meta);
    b.addEventListener("click", () => selectDesign(d.name));
    box.appendChild(b);
  }
}

function selectDesign(name) {
  selected = name;
  const d = designs.find((x) => x.name === name);
  paintDesigns();
  if (!d) return;
  $("f-name").textContent = d.name;
  $("f-lut").textContent = d.lut;
  $("f-pe").textContent = d.pe;
  $("f-reg").textContent = d.reg;
  $("f-rom").textContent = d.rom;
  const flags = [];
  if (d.blif) flags.push("blif");
  if (d.qasm) flags.push("qasm");
  flags.push("verilog", "vhdl", "tb", "vtb");
  if (d.rom !== "0") flags.push("rom");
  $("f-emit").textContent = flags.join(", ");
  $("f-what").textContent = d.what;
  const path = $("blif-path");
  if (d.blif) {
    path.value = "C:\\eda\\" + d.name + ".blif";
    path.placeholder = "";
  } else {
    path.value = "";
    path.placeholder = "no BLIF emitter on this design";
  }
}

async function runShow(pane, line, ms) {
  setBusy(true);
  setStatus(line, "busy");
  try {
    const text = await engine.shell(line, ms);
    $(pane).textContent = text;
    setStatus("ready", "ready");
    return text;
  } catch (e) {
    $(pane).textContent = String(e && e.message ? e.message : e);
    setStatus("the engine did not answer", "fail");
    return "";
  } finally {
    setBusy(false);
  }
}

async function refreshLibrary() {
  const text = await runShow("eda-out", "gpu fabric eda list", 120000);
  if (!text) return;
  designs = parseList(text);
  if (!selected || !designs.some((d) => d.name === selected)) {
    selected = designs.length ? designs[0].name : "";
  }
  if (selected) selectDesign(selected);
  else paintDesigns();
}

async function emitDesign() {
  if (!/^[A-Za-z_][A-Za-z0-9_]*$/.test(selected)) {
    $("eda-out").textContent = "Pick a design from the library.";
    return;
  }
  const fmt = $("fmt").value;
  if (!Object.prototype.hasOwnProperty.call(formats, fmt)) return;
  const text = await runShow("eda-out", "gpu fabric eda " + selected + " " + fmt, 120000);
  if (!text || fmt === "all") {
    $("eda-src").textContent = fmt === "all" ? "Every format was written under C:\\eda. Download takes the folder." : "";
    return;
  }
  const path = "C:\\eda\\" + selected + formats[fmt];
  const body = await runShow("eda-src", "type " + path, 60000);
  if (body && body.length >= 16000) {
    $("eda-src").textContent = body + "\n\n[console reply is capped; download C:\\eda for the file]";
  }
}

async function importBlif() {
  const path = $("blif-path").value.trim();
  if (!path || /[\r\n"]/.test(path) || path.indexOf(" ") >= 0) {
    $("eda-out").textContent = "Import takes one guest path, with no spaces. Example: C:\\eda\\cells.blif";
    return;
  }
  await runShow("eda-out", "gpu fabric eda import " + path, 120000);
}

// SKY130 signoff, the same reports the lab's silicon page reads.
const plots = new Map();
const chipColor = [C.teal, C.violet, C.amber, C.pink];

function factRow(cells) {
  const tr = document.createElement("tr");
  for (let i = 0; i < cells.length; i++) {
    const td = document.createElement("td");
    if (i === 0) td.className = "k";
    td.textContent = cells[i];
    tr.appendChild(td);
  }
  return tr;
}

function dieOf(m) {
  const box = String(m["design__die__bbox"] || "").split(" ");
  if (box.length < 4) return "—";
  return (+box[2]).toFixed(0) + " × " + (+box[3]).toFixed(0) + " µm";
}

function fixed(x, d) {
  const n = +x;
  if (Number.isNaN(n)) return "—";
  return n.toFixed(d);
}

async function powerMilli(path) {
  const r = await fetch(path, { cache: "no-store" });
  if (!r.ok) throw new Error(path + ": " + r.status);
  const line = (await r.text()).split("\n").find((l) => l.startsWith("Total"));
  if (!line) throw new Error(path + ": no Total line");
  return +line.trim().split(/\s+/)[4] * 1000;
}

async function loadSilicon() {
  const cfg = await (await fetch("/qdk/ui.json", { cache: "no-store" })).json();
  const silicon = cfg.silicon;
  const page = (cfg.pages || []).find((p) => p.id === "silicon");
  if (page) {
    if (page.heading) $("si-heading").textContent = page.heading;
    if (page.blurb) $("si-blurb").textContent = page.blurb;
    for (const [id, text] of Object.entries(page.cards || {})) {
      const el = $(id);
      if (el) el.textContent = text;
    }
  }
  const chips = silicon.chips;
  const metrics = await Promise.all(chips.map(async (c) => {
    const r = await fetch(c.metrics, { cache: "no-store" });
    if (!r.ok) throw new Error(c.metrics + ": " + r.status);
    return r.json();
  }));
  chips.forEach((c) => {
    const img = $(c.imageId);
    if (img) {
      img.src = c.layout;
      img.alt = c.layoutAlt || c.name;
    }
    const card = $(c.cardId);
    if (card) card.textContent = c.name;
  });
  const corners = silicon.corners;
  const series = (key) => metrics.map((m) => corners.map((c) => +m[key + c]));
  const tags = ["tt", "ss", "ff"];
  const power = await Promise.all(chips.map(async (c) => {
    const out = [];
    for (const tag of tags) {
      const path = String(c.power).includes(".tt.rpt") ? c.power.replace(".tt.rpt", "." + tag + ".rpt") : c.power;
      out.push(await powerMilli(path));
    }
    return out;
  }));
  return { chips, metrics, corners, slack: series("timing__setup__ws__corner:"), skew: series("clock__skew__worst_setup__corner:"), power, tags };
}

const siliconData = loadSilicon();

function bars(id, labels, series, unit) {
  const p = plots.get(id);
  const T = p.fit();
  const W = p.w, H = p.h;
  if (W < 8 || H < 8) return;
  let lo = Infinity, hi = -Infinity;
  for (const s of series) for (const v of s) {
    if (v < lo) lo = v;
    if (v > hi) hi = v;
  }
  if (lo > 0) lo = 0;
  if (hi < 0) hi = 0;
  if (hi <= lo) hi = lo + 1;
  const L = 54, R = 14, Tp = 16, B = 78;
  const ph = H - Tp - B, pwid = W - L - R;
  const yOf = (v) => Tp + ph - ((v - lo) / (hi - lo)) * ph;
  const g = new Prims(), rects = new Prims(), glow = new Prims();
  for (let i = 0; i <= 4; i++) {
    const y = Tp + (1 - i / 4) * ph;
    g.rect(L, y - 0.5, L + pwid, y + 0.5, C.grid);
  }
  if (lo < 0 && hi > 0) {
    const z = yOf(0);
    g.rect(L, z - 0.5, L + pwid, z + 0.5, C.dim);
  }
  const gw = pwid / labels.length;
  const n = series.length;
  labels.forEach((_, i) => {
    series.forEach((s, j) => {
      const val = s[i];
      const x0 = L + i * gw + gw * 0.16 + j * (gw * 0.7 / n);
      const x1 = x0 + (gw * 0.62) / n;
      const col = chipColor[j % chipColor.length];
      rects.rect(x0, yOf(val), x1, yOf(0), rgba(col, 0.88));
      glow.disc((x0 + x1) / 2, yOf(val), 3, 2, col);
    });
  });
  p.draw([{ prims: g }, { prims: rects }, { prims: glow, add: true }]);
  T.fillStyle = C.muted;
  T.font = "10.5px ui-monospace, Consolas, monospace";
  T.textAlign = "right";
  for (let i = 0; i <= 4; i++) {
    const v = lo + (i / 4) * (hi - lo);
    T.fillText(v.toFixed(hi - lo < 2 ? 2 : 1), L - 6, Tp + ph - (i / 4) * ph + 4);
  }
  T.fillText(unit, L - 6, 12);
  labels.forEach((c, i) => {
    T.save();
    T.translate(L + (i + 0.5) * gw, Tp + ph + 10);
    T.rotate(-Math.PI / 5);
    T.textAlign = "right";
    T.fillText(c, 0, 0);
    T.restore();
  });
}

function paintFacts(data) {
  const table = $("si-facts");
  table.textContent = "";
  const names = data.chips.map((c) => c.name);
  table.appendChild(factRow(["", ...names]));
  const col = (fn) => data.metrics.map(fn);
  const rows = [
    ["cells", col((m) => (+m["design__instance__count__stdcell"]).toLocaleString())],
    ["die", col(dieOf)],
    ["worst setup", col((m) => "+" + fixed(m["timing__setup__ws"], 2) + " ns")],
    ["slew / cap viol.", col((m) => m["design__max_slew_violation__count"] + " / " + m["design__max_cap_violation__count"])],
    ["DRC (Magic/KLayout)", col((m) => m["magic__drc_error__count"] + " / " + m["klayout__drc_error__count"])],
    ["LVS errors", col((m) => String(m["design__lvs_error__count"]))],
    ["power tt", data.power.map((p) => fixed(p[0], 2) + " mW")],
  ];
  for (const row of rows) table.appendChild(factRow([row[0], ...row[1]]));
}

async function drawSilicon() {
  if (!$("view-si").classList.contains("on")) return;
  try {
    const data = await siliconData;
    for (const id of ["si-slack", "si-power", "si-skew"]) {
      if (!plots.has(id)) plots.set(id, new Plot($(id)));
    }
    await Promise.all(["si-slack", "si-power", "si-skew"].map((id) => plots.get(id).ready));
    $("si-err").textContent = "";
    bars("si-slack", data.corners, data.slack, "ns");
    bars("si-power", data.tags, data.power, "mW");
    bars("si-skew", data.corners, data.skew, "ns");
    paintFacts(data);
  } catch (e) {
    $("si-err").textContent = e && e.message ? e.message : String(e);
  }
}

window.addEventListener("resize", () => {
  if ($("view-si").classList.contains("on")) drawSilicon();
});

// OpenSTA report_checks. The file lists worst paths first. Keep the first ones.
function parseSta(text, limit) {
  const lines = text.split(/\r?\n/);
  const paths = [];
  let cur = null;
  const flush = () => {
    if (cur && cur.start) paths.push(cur);
  };
  for (const line of lines) {
    if (line.startsWith("Startpoint:")) {
      flush();
      if (paths.length >= limit) {
        cur = null;
        break;
      }
      cur = { start: line.slice("Startpoint:".length).trim(), end: "", slack: "", met: true, arrival: "", required: "", body: [] };
      continue;
    }
    if (!cur) continue;
    cur.body.push(line);
    if (line.startsWith("Endpoint:")) cur.end = line.slice("Endpoint:".length).trim();
    const slack = line.match(/(-?\d+\.\d+)\s+slack \((MET|VIOLATED)\)/);
    if (slack) {
      cur.slack = slack[1];
      cur.met = slack[2] === "MET";
    }
    if (!cur.arrival) {
      const m = line.match(/(-?\d+\.\d+)\s+data arrival time/);
      if (m) cur.arrival = m[1];
    }
    if (!cur.required) {
      const m = line.match(/(-?\d+\.\d+)\s+data required time/);
      if (m) cur.required = m[1];
    }
  }
  flush();
  return paths.slice(0, limit);
}

let timingCfg = null;
let timingPaths = [];
let timingGen = 0;

function fillSelect(id, options, prev) {
  const sel = $(id);
  sel.textContent = "";
  for (const o of options) {
    const opt = document.createElement("option");
    opt.value = o.value;
    opt.textContent = o.label;
    sel.appendChild(opt);
  }
  if (prev && [...sel.options].some((o) => o.value === prev)) sel.value = prev;
}

async function timingConfig() {
  if (timingCfg) return timingCfg;
  const cfg = await (await fetch("/qdk/ui.json", { cache: "no-store" })).json();
  const chips = (cfg.silicon.chips || []).filter((c) => c.reports);
  timingCfg = { chips, corners: cfg.silicon.corners || [] };
  return timingCfg;
}

async function showTiming() {
  if (!$("view-time").classList.contains("on")) return;
  try {
    const cfg = await timingConfig();
    if (!$("time-chip").options.length) {
      fillSelect("time-chip", cfg.chips.map((c) => ({ value: c.reports, label: c.name })));
      fillSelect("time-corner", cfg.corners.map((c) => ({ value: c, label: c })), "nom_tt_025C_1v80");
    }
    $("time-err").textContent = "";
    if (!timingPaths.length) await loadTiming();
  } catch (e) {
    $("time-err").textContent = e && e.message ? e.message : String(e);
  }
}

function paintTiming(paths, meta) {
  const sum = $("time-sum");
  sum.textContent = "";
  const worst = paths.reduce((w, p) => {
    const s = +p.slack;
    return Number.isNaN(s) ? w : Math.min(w, s);
  }, Infinity);
  const violated = paths.filter((p) => !p.met).length;
  for (const row of [
    ["report", meta.report],
    ["paths shown", String(paths.length)],
    ["worst slack shown", worst === Infinity ? "—" : worst.toFixed(4) + " ns"],
    ["violated in view", String(violated)],
  ]) sum.appendChild(factRow(row));
  const body = $("time-rows");
  body.textContent = "";
  paths.forEach((p, i) => {
    const tr = document.createElement("tr");
    const cells = [String(i + 1), p.start, p.end, p.slack || "—", p.arrival || "—", p.required || "—"];
    cells.forEach((text, k) => {
      const td = document.createElement("td");
      td.textContent = text;
      if (k === 1 || k === 2) td.className = "pin";
      if (k === 3) td.className = p.met ? "met" : "viol";
      tr.appendChild(td);
    });
    tr.addEventListener("click", () => {
      for (const row of body.children) row.classList.remove("on");
      tr.classList.add("on");
      $("time-detail").textContent = p.start + "\n" + p.body.join("\n");
    });
    body.appendChild(tr);
  });
  $("time-detail").textContent = paths.length ? "Select a path." : "The report has no paths.";
}

async function loadTiming() {
  const gen = ++timingGen;
  const btn = $("time-load");
  btn.disabled = true;
  $("time-err").textContent = "";
  try {
    const cfg = await timingConfig();
    if (!$("time-chip").options.length) {
      fillSelect("time-chip", cfg.chips.map((c) => ({ value: c.reports, label: c.name })));
      fillSelect("time-corner", cfg.corners.map((c) => ({ value: c, label: c })), "nom_tt_025C_1v80");
    }
    const root = $("time-chip").value;
    const corner = $("time-corner").value;
    const kind = $("time-kind").value || "max";
    if (!root || !corner) throw new Error("no signoff reports are configured");
    const report = root.replace(/\/$/, "") + "/sta_" + corner + "/" + kind + ".rpt";
    const res = await fetch(report, { cache: "no-store" });
    if (!res.ok) throw new Error(report + ": " + res.status);
    const paths = parseSta(await res.text(), 24);
    if (gen !== timingGen) return;
    timingPaths = paths;
    paintTiming(timingPaths, { report });
  } catch (e) {
    if (gen !== timingGen) return;
    timingPaths = [];
    $("time-err").textContent = e && e.message ? e.message : String(e);
  } finally {
    if (gen === timingGen) btn.disabled = false;
  }
}

function bind() {
  $("nav-vm").addEventListener("click", () => show("vm"));
  $("nav-eda").addEventListener("click", () => show("eda"));
  $("nav-si").addEventListener("click", () => show("si"));
  $("nav-time").addEventListener("click", () => show("time"));
  $("time-load").addEventListener("click", () => loadTiming());
  for (const id of ["time-chip", "time-corner", "time-kind"]) $(id).addEventListener("change", () => loadTiming());
  $("hv-run").addEventListener("click", () => runShow("vm-out", "hv", 60000));
  $("wsl-status").addEventListener("click", () => runShow("vm-out", "wsl --status", 60000));
  $("wsl-list").addEventListener("click", () => runShow("vm-out", "wsl -l", 60000));
  $("wsl-boot").addEventListener("click", () => runShow("vm-out", "wsl-alpine", 180000));
  $("eda-refresh").addEventListener("click", () => refreshLibrary());
  $("emit").addEventListener("click", () => emitDesign());
  $("check").addEventListener("click", () => runShow("eda-out", "gpu fabric eda check", 180000));
  $("download").addEventListener("click", () => runShow("eda-out", "download C:\\eda", 120000));
  $("import").addEventListener("click", () => importBlif());
}

bind();
engine = new Engine(setStatus);
engine
  .boot()
  .then(() => {
    setStatus("ready", "ready");
    return engine.shell("hv", 60000);
  })
  .then((text) => {
    $("vm-out").textContent = text;
    return refreshLibrary();
  })
  .catch((e) => {
    setStatus(e && e.message ? e.message : String(e), "fail");
    $("vm-out").textContent = e && e.message ? e.message : String(e);
  });
