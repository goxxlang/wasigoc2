import { inspect, instantiate, run, call } from "./loader.js";

const $ = (id) => document.getElementById(id);
const drop = $("drop");
const inspectEl = $("inspect");
const outEl = $("out");
const fnSel = $("fn");
let current = null;

function log(kind, text) {
  const line = document.createElement("div");
  line.className = kind;
  line.textContent = text;
  outEl.appendChild(line);
  outEl.scrollTop = outEl.scrollHeight;
}

function showInspect(info) {
  const lines = [];
  lines.push("size " + info.size);
  lines.push("imports");
  for (const i of info.imports) lines.push("  " + i.module + "." + i.name + " " + i.kind);
  lines.push("exports");
  for (const e of info.exports) lines.push("  " + e.name + " " + e.kind);
  inspectEl.textContent = lines.join("\n");
  fnSel.innerHTML = "";
  const names = info.exports.filter((e) => e.kind === "function").map((e) => e.name);
  const order = [];
  if (names.includes("_start")) order.push("_start");
  for (const n of names) if (n !== "_start") order.push(n);
  for (const n of order) {
    const opt = document.createElement("option");
    opt.value = n;
    opt.textContent = n;
    fnSel.appendChild(opt);
  }
}

async function loadBytes(name, buf) {
  current = { name, bytes: buf };
  const info = inspect(buf);
  current.info = info;
  $("meta").textContent = name + "  " + buf.byteLength + " B";
  showInspect(info);
  log("ok", "loaded " + name);
}

drop.addEventListener("dragover", (e) => { e.preventDefault(); drop.classList.add("over"); });
drop.addEventListener("dragleave", () => drop.classList.remove("over"));
drop.addEventListener("drop", async (e) => {
  e.preventDefault();
  drop.classList.remove("over");
  const f = e.dataTransfer.files[0];
  if (f) await loadBytes(f.name, await f.arrayBuffer());
});
$("file").addEventListener("change", async (e) => {
  const f = e.target.files[0];
  if (f) await loadBytes(f.name, await f.arrayBuffer());
  e.target.value = "";
});

$("go").addEventListener("submit", async (e) => {
  e.preventDefault();
  if (!current) {
    log("err", "load a .wasm");
    return;
  }
  const name = fnSel.value;
  const args = $("args").value.trim().split(/\s+/).filter(Boolean).map((x) => parseInt(x, 10) || 0);
  try {
    const inst = await instantiate(current.bytes, {
      args: [current.name],
      stdout: (t) => log("ok", t.replace(/\n$/, "")),
      stderr: (t) => log("err", t.replace(/\n$/, "")),
    });
    if (name === "_start" || !name) {
      const code = await run(inst);
      log("ok", "exit " + code);
    } else {
      const got = await call(inst, name, args);
      log("ok", String(got));
    }
  } catch (err) {
    log("err", err && err.message ? err.message : String(err));
  }
});
