// Local chat. bitnet.wasm is the guest. Dawn (the webgpu package) is the GPU.
// A terminal writes a screen. A pipe keeps the raw guest stream (probe.mjs).
//
//   node hodge.mjs C:\Users\grego\models\dev\hodge-4
import { Worker } from "node:worker_threads";
import { spawn } from "node:child_process";
import { createInterface, emitKeypressEvents } from "node:readline";
import { fileURLToPath } from "node:url";
import { dirname, join, resolve, basename } from "node:path";
import { existsSync } from "node:fs";

if (typeof WebAssembly.Suspending !== "function") {
  const child = spawn(process.execPath, ["--experimental-wasm-jspi", ...process.argv.slice(1)], { stdio: "inherit" });
  child.on("exit", (code) => process.exit(code == null ? 1 : code));
  process.exitCode = 0;
} else {
  await main();
}

async function main() {
  const here = dirname(fileURLToPath(import.meta.url));
  const model = resolve(process.argv[2] || "");
  if (!process.argv[2] || !existsSync(join(model, "model.safetensors")) || !existsSync(join(model, "config.json"))) {
    console.error("usage: node hodge.mjs <model-dir>");
    console.error("the directory needs config.json and model.safetensors");
    process.exit(2);
  }

  const sab = new SharedArrayBuffer(1 << 16);
  const i32 = new Int32Array(sab);
  const u8 = new Uint8Array(sab);
  const worker = new Worker(new URL("./guest.mjs", import.meta.url), {
    workerData: {
      sab,
      model,
      kernels: join(here, "..", "kernels"),
      wasm: join(here, "..", "web", "bitnet.wasm"),
    },
  });

  function push(text) {
    const b = Buffer.from(text, "utf8");
    if (16 + b.length > u8.length) return false;
    while (Atomics.load(i32, 1) !== 0) Atomics.wait(i32, 1, Atomics.load(i32, 1), 50);
    u8.set(b, 16);
    Atomics.store(i32, 1, b.length);
    Atomics.store(i32, 0, 1);
    Atomics.notify(i32, 0, 1);
    return true;
  }

  const tty = Boolean(process.stdin.isTTY && process.stdout.isTTY);
  if (!tty) {
    worker.on("message", (m) => {
      if (!m) return;
      if (m.type === "out") process.stdout.write(m.text);
      else if (m.type === "err") process.stderr.write(m.text);
      else if (m.type === "exit") process.exit(m.code | 0);
    });
    worker.on("error", (err) => {
      console.error(err);
      process.exit(1);
    });
    worker.on("exit", (code) => process.exit(code || 0));
    const rl = createInterface({ input: process.stdin, terminal: false });
    rl.on("line", (line) => push(line + "\n"));
    rl.on("close", () => push(""));
    return;
  }

  const ui = screen(basename(model));
  let leaving = false;
  function quit(code) {
    if (leaving) return;
    leaving = true;
    ui.restore();
    process.exit(code || 0);
  }

  worker.on("message", (m) => {
    if (!m) return;
    if (m.type === "out") ui.out(m.text);
    else if (m.type === "err") ui.err(m.text);
    else if (m.type === "exit") quit(m.code | 0);
  });
  worker.on("error", (err) => {
    ui.restore();
    console.error(err);
    process.exit(1);
  });
  worker.on("exit", (code) => quit(code || 0));

  emitKeypressEvents(process.stdin);
  process.stdin.setRawMode(true);
  process.stdin.resume();
  process.on("SIGINT", () => quit(0));
  process.on("exit", () => ui.restore());
  process.stdout.on("resize", () => ui.draw());

  ui.draw();

  process.stdin.on("keypress", (str, key) => {
    if (!key) return;
    if ((key.ctrl && key.name === "c") || (key.ctrl && key.name === "d")) {
      quit(0);
      return;
    }
    if (key.name === "return") {
      ui.submit(push);
      return;
    }
    if (key.name === "backspace") {
      ui.backspace();
      return;
    }
    if (key.ctrl || key.meta) return;
    if (str && (str.includes("\r") || str.includes("\n"))) {
      ui.paste(str, push);
      return;
    }
    if (str && str >= " ") ui.type(str);
  });
}

function screen(name) {
  const messages = [];
  let info = "";
  let stats = "";
  let status = "loading";
  let ready = false;
  let generating = false;
  let eatPrompt = false;
  let sawPrompt = false;
  let draft = "";
  let hold = "";
  let errHold = "";
  let note = "enter sends, ctrl+c quits";
  let on = false;

  function restore() {
    if (!on) return;
    on = false;
    process.stdout.write("\x1b[?1049l\x1b[?25h");
    try {
      process.stdin.setRawMode(false);
    } catch {
      /* already restored */
    }
  }

  function open() {
    if (on) return;
    on = true;
    process.stdout.write("\x1b[?1049h\x1b[H");
  }

  function cols() {
    return Math.max(20, process.stdout.columns || 80);
  }
  function rows() {
    return Math.max(8, process.stdout.rows || 24);
  }

  function partialPrompt(s) {
    if (s.endsWith("> ")) return "";
    if (s.endsWith("\n>")) return "\n>";
    if (s.endsWith(">")) return ">";
    return "";
  }

  function out(text) {
    hold += text;
    if (!generating) {
      if (hold.includes("> ")) sawPrompt = true;
      hold = partialPrompt(hold);
      return;
    }
    if (eatPrompt) {
      if (hold === ">") return;
      if (hold.startsWith("> ")) hold = hold.slice(2);
      eatPrompt = false;
    }
    if (!hold) {
      draw();
      return;
    }
    const msg = messages[messages.length - 1];
    if (msg && msg.role === "hodge") msg.text += hold;
    hold = "";
    draw();
  }

  function err(text) {
    errHold += text;
    let i;
    while ((i = errHold.indexOf("\n")) >= 0) {
      const line = errHold.slice(0, i).replace(/\r$/, "");
      errHold = errHold.slice(i + 1);
      errLine(line);
    }
  }

  function errLine(line) {
    if (!line) return;
    if (line.includes("type a message")) {
      ready = true;
      if (status === "loading") status = "ready";
      note = "enter sends, ctrl+c quits";
      draw();
      return;
    }
    if (/^\[\d+ prompt tokens in /.test(line)) {
      stats = line.slice(1, -1);
      generating = false;
      eatPrompt = false;
      const msg = messages[messages.length - 1];
      if (msg && msg.role === "hodge") msg.text = msg.text.replace(/\n$/, "");
      status = "ready";
      note = "enter sends, ctrl+c quits";
      draw();
      return;
    }
    if (!ready) {
      info = line;
      draw();
      return;
    }
    messages.push({ role: "err", text: line });
    status = "error";
    note = line;
    draw();
  }

  function submit(push) {
    if (!ready || generating) {
      note = ready ? "wait for this answer" : "still loading the model";
      draw();
      return;
    }
    const line = draft;
    draft = "";
    if (!line.trim()) {
      draw();
      return;
    }
    if (!push(line + "\n")) {
      draft = line;
      note = "that line is too long";
      draw();
      return;
    }
    messages.push({ role: "you", text: line });
    messages.push({ role: "hodge", text: "" });
    generating = true;
    eatPrompt = !sawPrompt;
    sawPrompt = false;
    hold = "";
    status = "generating";
    note = "generating";
    draw();
  }

  function paste(str, push) {
    const parts = str.split(/\r\n|\r|\n/);
    draft += parts[0];
    for (let i = 1; i < parts.length; i++) {
      submit(push);
      draft = parts[i];
    }
    draw();
  }

  function backspace() {
    const rs = [...draft];
    rs.pop();
    draft = rs.join("");
    draw();
  }

  function type(str) {
    draft += str;
    draw();
  }

  function draw() {
    open();
    const c = cols();
    const r = rows();
    const promptLines = wrap("> " + draft, c).slice(-2);
    const room = r - 3 - promptLines.length;
    const body = [];
    for (const m of messages) {
      body.push({ kind: m.role, text: m.role === "you" ? "you" : m.role === "err" ? "error" : "hodge" });
      const text = m.text || (m.role === "hodge" && generating ? "…" : "");
      const bits = text.split("\n");
      for (const bit of bits) {
        const wrapped = wrap(bit ? "  " + bit : "  ", c);
        for (const w of wrapped) body.push({ kind: "text", text: w });
      }
      body.push({ kind: "text", text: "" });
    }
    const view = body.slice(Math.max(0, body.length - Math.max(1, room)));
    while (view.length < room) view.unshift({ kind: "text", text: "" });

    const lines = [
      { kind: "title", text: fit("hodge  " + name + "  " + status, c) },
      { kind: "dim", text: fit(info || "loading weights", c) },
      ...view.slice(0, room),
      { kind: "dim", text: fit(stats || note, c) },
      ...promptLines.map((text, i) => ({ kind: "prompt", text: i === promptLines.length - 1 ? text : text })),
    ];
    while (lines.length < r) lines.push({ kind: "text", text: "" });
    lines.length = r;

    let buf = "\x1b[H";
    for (let i = 0; i < r; i++) {
      buf += "\x1b[2K" + paint(lines[i]);
      if (i < r - 1) buf += "\n";
    }
    const last = lines[r - 1].text;
    const col = Math.min(c, [...last].length) + 1;
    buf += "\x1b[" + r + ";" + col + "H";
    process.stdout.write(buf);
  }

  return { out, err, submit, paste, backspace, type, draw, restore };
}

function paint(line) {
  const s = line.text;
  if (line.kind === "title") return "\x1b[1m" + s + "\x1b[0m";
  if (line.kind === "you") return "\x1b[36m" + s + "\x1b[0m";
  if (line.kind === "hodge") return "\x1b[33m" + s + "\x1b[0m";
  if (line.kind === "err") return "\x1b[31m" + s + "\x1b[0m";
  if (line.kind === "dim") return "\x1b[2m" + s + "\x1b[0m";
  if (line.kind === "prompt") return s;
  return s;
}

function fit(s, n) {
  const rs = [...s];
  if (rs.length <= n) return s;
  return rs.slice(0, Math.max(0, n - 1)).join("") + "…";
}

function wrap(s, width) {
  if (width < 1) width = 1;
  const rs = [...s];
  if (rs.length <= width) return [s];
  const out = [];
  let cur = "";
  let n = 0;
  const parts = s.split(/(\s+)/);
  for (const w of parts) {
    const wn = [...w].length;
    if (n + wn <= width) {
      cur += w;
      n += wn;
      continue;
    }
    if (cur) out.push(cur.replace(/\s+$/, ""));
    if (w.trim() === "") {
      cur = "";
      n = 0;
      continue;
    }
    if (wn > width) {
      const chars = [...w];
      let i = 0;
      while (i < chars.length) {
        out.push(chars.slice(i, i + width).join(""));
        i += width;
      }
      cur = "";
      n = 0;
      continue;
    }
    cur = w.trimStart();
    n = [...cur].length;
  }
  if (cur) out.push(cur.replace(/\s+$/, ""));
  return out.length ? out : [""];
}
