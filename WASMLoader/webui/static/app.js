const modules = [];
const $ = (id) => document.getElementById(id);

const drop = $("drop");
const list = $("list");
const inspectEl = $("inspect");
const outEl = $("out");
const modSel = $("mod");
const fnSel = $("fn");

function log(kind, text) {
  const line = document.createElement("div");
  line.className = kind;
  line.textContent = text;
  outEl.appendChild(line);
  outEl.scrollTop = outEl.scrollHeight;
}

function renderList() {
  list.innerHTML = "";
  modSel.innerHTML = "";
  modules.forEach((m, i) => {
    const li = document.createElement("li");
    if (m.active) li.classList.add("active");
    const name = document.createElement("input");
    name.value = m.name;
    name.title = "module name (imports resolve against this)";
    name.addEventListener("change", () => {
      m.name = name.value.trim() || m.name;
      renderList();
    });
    name.addEventListener("click", (e) => e.stopPropagation());
    const sz = document.createElement("span");
    sz.textContent = m.bytes.byteLength + " B";
    const meta = document.createElement("div");
    meta.className = "meta";
    meta.textContent = m.fileName;
    li.append(name, sz, meta);
    li.addEventListener("click", () => select(i));
    list.appendChild(li);

    const opt = document.createElement("option");
    opt.value = m.name;
    opt.textContent = m.name;
    modSel.appendChild(opt);
  });
  refreshFns();
}

function select(i) {
  modules.forEach((m, j) => { m.active = j === i; });
  renderList();
  inspect(modules[i]);
}

async function inspect(m) {
  if (!m) {
    inspectEl.textContent = "no module selected";
    return;
  }
  const res = await fetch("/api/inspect", {
    method: "POST",
    headers: { "Content-Type": "application/wasm", "X-Wasm-Name": m.fileName },
    body: m.bytes,
  });
  const text = await res.text();
  inspectEl.textContent = text;
  if (!res.ok) log("err", text);
}

function refreshFns() {
  const name = modSel.value;
  const m = modules.find((x) => x.name === name);
  fnSel.innerHTML = "";
  const exports = (m && m.exports) || [];
  for (const ex of exports) {
    const opt = document.createElement("option");
    opt.value = ex;
    opt.textContent = ex;
    fnSel.appendChild(opt);
  }
}

async function addFile(file) {
  const bytes = await file.arrayBuffer();
  await addModule(file.name.replace(/\.wasm$/i, "") || "mod", file.name, bytes);
}

async function addModule(name, fileName, bytes) {
  const res = await fetch("/api/inspect.json", {
    method: "POST",
    headers: { "Content-Type": "application/wasm" },
    body: bytes,
  });
  const info = await res.json();
  if (!res.ok) {
    log("err", info.error || "inspect failed");
    return;
  }
  const exports = (info.exports || []).filter((e) => e.kind === "func").map((e) => e.name);
  modules.push({ name, fileName, bytes, exports, active: false });
  select(modules.length - 1);
  log("ok", "queued " + name + " (" + fileName + ")");
}

drop.addEventListener("dragover", (e) => { e.preventDefault(); drop.classList.add("over"); });
drop.addEventListener("dragleave", () => drop.classList.remove("over"));
drop.addEventListener("drop", async (e) => {
  e.preventDefault();
  drop.classList.remove("over");
  for (const file of e.dataTransfer.files) await addFile(file);
});
$("file").addEventListener("change", async (e) => {
  for (const file of e.target.files) await addFile(file);
  e.target.value = "";
});

document.querySelectorAll("[data-demo]").forEach((btn) => {
  btn.addEventListener("click", async () => {
    const name = btn.dataset.demo;
    const res = await fetch("/api/example/" + name);
    if (!res.ok) {
      log("err", await res.text());
      return;
    }
    const bytes = await res.arrayBuffer();
    const instName = name === "add" ? "math" : name === "double" ? "app" : name;
    await addModule(instName, name + ".wasm", bytes);
  });
});

modSel.addEventListener("change", refreshFns);

$("call").addEventListener("submit", async (e) => {
  e.preventDefault();
  if (modules.length === 0) {
    log("err", "load at least one .wasm");
    return;
  }
  const fd = new FormData();
  for (const m of modules) {
    fd.append("name", m.name);
    fd.append("wasm", new Blob([m.bytes], { type: "application/wasm" }), m.fileName);
  }
  fd.set("call", modSel.value + "." + fnSel.value);
  fd.set("args", $("args").value.trim());
  const res = await fetch("/api/link", { method: "POST", body: fd });
  const text = await res.text();
  log(res.ok ? "ok" : "err", text);
});
