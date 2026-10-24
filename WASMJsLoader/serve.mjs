#!/usr/bin/env node
// Static hop for WASMJsLoader. COOP/COEP so SharedArrayBuffer works
// if the guest was built with threads. The guest Calls loader.js;
// this process only serves bytes.
import http from "node:http";
import fs from "node:fs";
import path from "node:path";
import zlib from "node:zlib";
import { Worker } from "node:worker_threads";
import { fileURLToPath, pathToFileURL } from "node:url";
import { wrapMimicryWasm, peelMimicryWasm } from "./loader.js";

const root = path.dirname(fileURLToPath(import.meta.url));
const home = process.env.USERPROFILE || process.env.HOME || "";
const port = Number(process.env.PORT || 8792);
const types = {
  ".html": "text/html; charset=utf-8",
  ".js": "text/javascript; charset=utf-8",
  ".mjs": "text/javascript; charset=utf-8",
  ".css": "text/css; charset=utf-8",
  ".wasm": "application/wasm",
  ".json": "application/json",
  ".webmanifest": "application/manifest+json",
  ".png": "image/png",
  ".wgsl": "text/plain; charset=utf-8",
  ".txt": "text/plain; charset=utf-8",
};

// Mounts: URL prefix -> directory. `/webgpu/` is WASMWebGPU's JS host (this
// repo); more come from `--mount /prefix=dir` (repeatable), e.g. a guest's
// page or a folder of model weights.
const mounts = [["/webgpu/", path.join(root, "..", "WASMWebGPU", "js")]];
let runtimeDir = "";
for (let i = 2; i < process.argv.length; i++) {
  const a = process.argv[i];
  if (a === "--runtime" || a.startsWith("--runtime=")) {
    runtimeDir = a === "--runtime" ? process.argv[++i] || "" : a.slice(10);
    continue;
  }
  const v = a === "--mount" ? process.argv[++i] : a.startsWith("--mount=") ? a.slice(8) : null;
  if (v == null) continue;
  const eq = v.indexOf("=");
  if (eq <= 0) {
    process.stderr.write("--mount wants /prefix=dir, not " + v + "\n");
    process.exit(2);
  }
  let prefix = v.slice(0, eq);
  if (!prefix.startsWith("/")) prefix = "/" + prefix;
  if (!prefix.endsWith("/")) prefix += "/";
  mounts.push([prefix, path.resolve(v.slice(eq + 1))]);
}

// The file a URL path names: under the longest matching mount, else this
// directory. Null when the path climbs out of its directory, either as
// written (a relative path that leaves it; a sibling folder sharing its name's
// prefix is outside too) or through a symlink inside it.
function resolveFile(rel) {
  if (rel.includes("\0")) return null;
  let best = null;
  for (const [prefix, dir] of mounts) {
    if (rel.startsWith(prefix) && (!best || prefix.length > best[0].length)) best = [prefix, dir];
  }
  const [prefix, dir] = best || ["/", root];
  const file = path.resolve(dir, "." + rel.slice(prefix.length - 1));
  if (!within(dir, file)) return null;
  try {
    if (!within(fs.realpathSync(dir), fs.realpathSync(file))) return null;
  } catch {
    // not there: the caller answers 404
  }
  return file;
}

function within(dir, file) {
  const back = path.relative(dir, file);
  return back === "" || (back !== ".." && !back.startsWith(".." + path.sep) && !path.isAbsolute(back));
}

const headers = (file, extra) =>
  Object.assign(
    {
      "Content-Type": types[path.extname(file)] || "application/octet-stream",
      "Cross-Origin-Opener-Policy": "same-origin",
      "Cross-Origin-Embedder-Policy": "require-corp",
      "Accept-Ranges": "bytes",
    },
    extra
  );

function firstExisting(names) {
  for (const p of names) {
    if (fs.existsSync(p)) return p;
  }
  return names[0];
}

function guestWasm() {
  return firstExisting([
    path.join(root, "guest", "cmdterm.wasm"),
    path.join(root, "..", "go++", "examples", "cmdterm", "cmdterm.wasm"),
    path.join(home, "go++", "examples", "cmdterm", "cmdterm.wasm"),
  ]);
}

function proofWasm() {
  return firstExisting([
    path.join(root, "guest", "proofshow.wasm"),
    path.join(root, "..", "examples", "proofshow", "proofshow.wasm"),
    path.join(home, "go++", "examples", "proofshow", "proofshow.wasm"),
  ]);
}

// In-memory WebRTC rooms for BNZ chunk delivery. POST offer replaces the
// room and clears any previous answer. /api/model/bytes gzips a slice of the
// one packed model (the codec is BNZ; gzip is only the wire layer); the
// request names no path, so nothing else can be read through it.
const rooms = new Map();
const MAX_BNZ_SLICE = 1024 * 1024;
const MODEL_FILE = "/models/hodge-1/model.bnz";

function readBody(req, limit) {
  return new Promise((resolve, reject) => {
    const chunks = [];
    let n = 0;
    req.on("data", (c) => {
      n += c.length;
      if (n > limit) {
        reject(new Error("big"));
        req.destroy();
        return;
      }
      chunks.push(c);
    });
    req.on("end", () => resolve(Buffer.concat(chunks)));
    req.on("error", reject);
  });
}

const coop = {
  "Cross-Origin-Opener-Policy": "same-origin",
  "Cross-Origin-Embedder-Policy": "require-corp",
  "Cache-Control": "no-store",
};

async function handleBnz(req, res, url) {
  if (url.pathname === "/api/model/bytes") {
    if (req.method !== "GET" && req.method !== "HEAD") {
      res.writeHead(405, coop);
      res.end();
      return;
    }
    const off = Number(url.searchParams.get("off") || "0");
    const n = Number(url.searchParams.get("n") || "0");
    if (!Number.isInteger(off) || off < 0 || !Number.isInteger(n) || n < 1 || n > MAX_BNZ_SLICE) {
      res.writeHead(400, coop);
      res.end();
      return;
    }
    const file = resolveFile(MODEL_FILE);
    if (!file) {
      res.writeHead(403, coop);
      res.end();
      return;
    }
    let st;
    try { st = fs.statSync(file); } catch {
      res.writeHead(404, coop);
      res.end();
      return;
    }
    if (!st.isFile() || off >= st.size) {
      res.writeHead(416, coop);
      res.end();
      return;
    }
    const take = Math.min(n, st.size - off);
    const buf = Buffer.alloc(take);
    const fd = fs.openSync(file, "r");
    try { fs.readSync(fd, buf, 0, take, off); }
    finally { fs.closeSync(fd); }
    const gz = zlib.gzipSync(buf);
    res.writeHead(200, {
      ...coop,
      "Content-Type": "application/octet-stream",
      "Content-Encoding": "gzip",
      "Content-Length": gz.length,
    });
    res.end(req.method === "HEAD" ? undefined : gz);
    return;
  }
  const m = /^\/api\/bnz\/([a-z0-9-]{1,32})\/(offer|answer)$/.exec(url.pathname);
  if (!m) {
    res.writeHead(404, coop);
    res.end();
    return;
  }
  const room = m[1];
  const which = m[2];
  if (req.method === "GET" || req.method === "HEAD") {
    const body = (rooms.get(room) || {})[which];
    if (!body) {
      res.writeHead(404, coop);
      res.end();
      return;
    }
    res.writeHead(200, { ...coop, "Content-Type": "text/plain; charset=utf-8", "Content-Length": Buffer.byteLength(body) });
    res.end(req.method === "HEAD" ? undefined : body);
    return;
  }
  if (req.method !== "POST") {
    res.writeHead(405, coop);
    res.end();
    return;
  }
  let raw;
  try { raw = await readBody(req, MAX_BNZ_SLICE); }
  catch {
    if (!res.headersSent) { res.writeHead(413, coop); res.end(); }
    return;
  }
  const text = raw.toString("utf8");
  let rec = rooms.get(room);
  if (!rec) { rec = { offer: "", answer: "" }; rooms.set(room, rec); }
  if (which === "offer") {
    rec.offer = text;
    rec.answer = "";
  } else rec.answer = text;
  res.writeHead(204, coop);
  res.end();
}

let champExt;
function loadChampExt() {
  if (!champExt) {
    const file = path.join(root, "..", "examples", "bitnet", "web", "search.mjs");
    champExt = fs.existsSync(file) ? import(pathToFileURL(file).href) : Promise.resolve(null);
  }
  return champExt;
}

function sendJson(res, code, obj) {
  const body = Buffer.from(JSON.stringify(obj));
  res.writeHead(code, {
    ...coop,
    "Content-Type": "application/json; charset=utf-8",
    "Content-Length": body.length,
  });
  res.end(body);
}

async function handleExt(req, res, url) {
  if (req.method !== "GET") {
    res.writeHead(405, coop);
    res.end();
    return;
  }
  const mod = await loadChampExt();
  if (!mod) {
    res.writeHead(404, coop);
    res.end();
    return;
  }
  try {
    if (url.pathname === "/api/ext/search") {
      const q = (url.searchParams.get("q") || "").trim();
      if (!q || q.length > 200) {
        sendJson(res, 400, { ok: false, output: "Missing search query." });
        return;
      }
      sendJson(res, 200, { ok: true, output: await mod.searchWeb(q) });
      return;
    }
    const target = (url.searchParams.get("url") || "").trim();
    if (!target || target.length > 2000) {
      sendJson(res, 400, { ok: false, output: "Missing address." });
      return;
    }
    const got = await mod.gatherPage(target);
    sendJson(res, 200, { ok: !!got.ok, output: got.output || "The lookup failed." });
  } catch {
    if (!res.headersSent) sendJson(res, 200, { ok: false, output: "The lookup failed." });
  }
}

// Hodge-4 runs in this process. The page streams /api/chat on this same port.
const runtime = {
  name: "hodge-4",
  ready: false,
  failed: "",
  sab: null,
  i32: null,
  u8: null,
  buf: "",
  pending: null,
  tail: Promise.resolve(),
};

function runtimePush(text) {
  const b = Buffer.from(text, "utf8");
  const i32 = runtime.i32;
  const u8 = runtime.u8;
  if (!i32 || 16 + b.length > u8.length) return false;
  const deadline = Date.now() + 600000;
  while (Atomics.load(i32, 1) !== 0) {
    if (Date.now() > deadline) return false;
    Atomics.wait(i32, 1, Atomics.load(i32, 1), 50);
  }
  u8.set(b, 16);
  Atomics.store(i32, 1, b.length);
  Atomics.store(i32, 0, 1);
  Atomics.notify(i32, 0, 1);
  return true;
}

function runtimeEvent(ev) {
  if (ev.ev === "ready") {
    runtime.ready = true;
    process.stderr.write("hodge-4 ready\n");
    return;
  }
  if (ev.ev === "progress") {
    process.stderr.write("hodge-4 " + Math.round((Number(ev.p) || 0) * 100) + "%\n");
    return;
  }
  const p = runtime.pending;
  if (!p) return;
  if (ev.ev === "token") p.onToken(ev.text || "");
  else if (ev.ev === "done") {
    runtime.pending = null;
    p.onDone(ev);
  } else if (ev.ev === "error") {
    runtime.pending = null;
    runtime.failed = ev.msg || "the runtime stopped";
    p.onError(new Error(runtime.failed));
  } else if (ev.ev === "notice") {
    runtime.pending = null;
    p.onError(new Error(ev.msg || "that message was not run"));
  }
}

function runtimeOut(text) {
  runtime.buf += text;
  let nl;
  while ((nl = runtime.buf.indexOf("\n")) >= 0) {
    const line = runtime.buf.slice(0, nl).trim();
    runtime.buf = runtime.buf.slice(nl + 1);
    if (!line) continue;
    try { runtimeEvent(JSON.parse(line)); }
    catch { process.stderr.write(line + "\n"); }
  }
}

function startRuntime(dir) {
  const sab = new SharedArrayBuffer(1 << 16);
  runtime.sab = sab;
  runtime.i32 = new Int32Array(sab);
  runtime.u8 = new Uint8Array(sab);
  const worker = new Worker(path.join(root, "..", "examples", "bitnet", "tui", "guest.mjs"), {
    workerData: {
      sab,
      ui: true,
      model: path.resolve(dir),
      kernels: path.join(root, "..", "examples", "bitnet", "kernels"),
      wasm: path.join(root, "..", "examples", "bitnet", "web", "bitnet.wasm"),
    },
  });
  worker.on("message", (m) => {
    if (!m) return;
    if (m.type === "out") runtimeOut(m.text);
    else if (m.type === "err") process.stdout.write(m.text);
    else if (m.type === "exit" && !runtime.ready) {
      runtime.failed = "runtime exited " + (m.code | 0);
      process.stdout.write(runtime.failed + "\n");
    }
  });
  worker.on("error", (err) => {
    runtime.failed = (err && err.stack) || String(err);
    process.stdout.write(runtime.failed + "\n");
  });
  process.stderr.write("hodge-4 loading " + dir + "\n");
}

function runtimeChat(text, system, max, onToken) {
  const run = runtime.tail.then(() => new Promise((resolve, reject) => {
    if (!runtime.ready) {
      reject(new Error(runtime.failed || "Hodge-4 is still loading"));
      return;
    }
    runtime.pending = {
      onToken,
      onDone(ev) { resolve(ev); },
      onError: reject,
    };
    const cmd = { op: "chat", text };
    if (system) cmd.system = system;
    if (max) cmd.max = max;
    if (!runtimePush(JSON.stringify(cmd) + "\n")) {
      runtime.pending = null;
      reject(new Error("Hodge-4 did not take the message"));
    }
  }));
  runtime.tail = run.then(() => {}, () => {});
  return run;
}

function stamp() {
  return String(Math.floor(Date.now() / 1000));
}

async function handleModelApi(req, res, url) {
  const p = url.pathname;
  if (req.method === "GET" && p === "/api/version") {
    sendJson(res, 200, { version: "bitnet" });
    return;
  }
  if (req.method === "GET" && (p === "/api/tags" || p === "/api/ps")) {
    if (!runtime.ready) {
      sendJson(res, 503, { error: runtime.failed || "Hodge-4 is still loading" });
      return;
    }
    sendJson(res, 200, { models: [{ name: runtime.name, model: runtime.name }] });
    return;
  }
  if (req.method !== "POST" || (p !== "/api/chat" && p !== "/api/generate")) {
    res.writeHead(404, coop);
    res.end("not found");
    return;
  }
  if (!runtime.ready) {
    sendJson(res, 503, { error: runtime.failed || "Hodge-4 is still loading" });
    return;
  }
  let raw;
  try { raw = await readBody(req, 1024 * 1024); }
  catch {
    if (!res.headersSent) { res.writeHead(413, coop); res.end(); }
    return;
  }
  let body;
  try { body = JSON.parse(raw.toString("utf8")); }
  catch {
    sendJson(res, 400, { error: "not JSON" });
    return;
  }
  const stream = body.stream !== false;
  let text = "";
  let system = "";
  if (p === "/api/generate") text = String(body.prompt || "");
  else {
    const msgs = Array.isArray(body.messages) ? body.messages : [];
    for (const m of msgs) {
      if (m && m.role === "system" && m.content) system = String(m.content);
    }
    const users = msgs.filter((m) => m && m.role === "user" && m.content);
    text = users.length ? String(users[users.length - 1].content) : "";
  }
  if (!text) {
    sendJson(res, 400, { error: "no messages" });
    return;
  }
  const max = body.options && Number(body.options.num_predict);
  const created = stamp();
  let answer = "";
  if (stream) {
    res.writeHead(200, { ...coop, "Content-Type": "application/x-ndjson; charset=utf-8" });
  }
  let ev;
  try {
    ev = await runtimeChat(text, system, max > 0 ? max : 0, (piece) => {
      if (!piece) return;
      answer += piece;
      if (!stream) return;
      const line = p === "/api/generate"
        ? { model: runtime.name, created_at: created, response: piece, done: false }
        : { model: runtime.name, created_at: created, message: { role: "assistant", content: piece }, done: false };
      res.write(JSON.stringify(line) + "\n");
    });
  } catch (err) {
    const msg = (err && err.message) || "Hodge-4 failed";
    if (stream && res.headersSent) {
      res.write(JSON.stringify({ error: msg }) + "\n");
      res.end();
      return;
    }
    if (!res.headersSent) sendJson(res, 503, { error: msg });
    return;
  }
  const done = {
    done: true,
    done_reason: (ev && ev.reason) || "eos",
    prompt_eval_count: (ev && ev.prompt) || 0,
    eval_count: (ev && ev.tokens) || 0,
    prompt_eval_duration: (ev && ev.prefill_ms) || 0,
    eval_duration: (ev && ev.decode_ms) || 0,
  };
  if (stream) {
    const line = p === "/api/generate"
      ? { model: runtime.name, created_at: created, response: "", ...done }
      : { model: runtime.name, created_at: created, message: { role: "assistant", content: "" }, ...done };
    res.write(JSON.stringify(line) + "\n");
    res.end();
    return;
  }
  if (p === "/api/generate") sendJson(res, 200, { model: runtime.name, created_at: created, response: answer, ...done });
  else sendJson(res, 200, { model: runtime.name, created_at: created, message: { role: "assistant", content: answer }, ...done });
}

const server = http.createServer(async (req, res) => {
  const url = new URL(req.url || "/", "http://127.0.0.1");
  if (runtimeDir && (url.pathname === "/api/version" || url.pathname === "/api/tags" || url.pathname === "/api/ps" || url.pathname === "/api/chat" || url.pathname === "/api/generate")) {
    try { await handleModelApi(req, res, url); }
    catch {
      if (!res.headersSent) sendJson(res, 500, { error: "Hodge-4 failed" });
    }
    return;
  }
  if (url.pathname === "/api/ext/search" || url.pathname === "/api/ext/gather") {
    try { await handleExt(req, res, url); }
    catch {
      if (!res.headersSent) { res.writeHead(500, coop); res.end(); }
    }
    return;
  }
  if (url.pathname.startsWith("/api/bnz/") || url.pathname === "/api/model/bytes") {
    try { await handleBnz(req, res, url); }
    catch {
      if (!res.headersSent) { res.writeHead(500, coop); res.end(); }
    }
    return;
  }
  let rel = decodeURIComponent(url.pathname);
  if (rel === "/") rel = "/index.html";
  let file;
  if (rel === "/guest/cmdterm.wasm" || rel === "/cmdterm.wasm") {
    file = guestWasm();
  } else if (rel === "/guest/proofshow.wasm" || rel === "/proofshow.wasm") {
    file = proofWasm();
  } else {
    file = resolveFile(rel);
    if (!file) {
      res.writeHead(403);
      res.end();
      return;
    }
    // Plain files stream, with HEAD and Range (a guest reads big files, such
    // as model weights, a piece at a time).
    fs.stat(file, (err, st) => {
      if (err || !st.isFile()) {
        res.writeHead(404);
        res.end("not found");
        return;
      }
      const m = /^bytes=(\d*)-(\d*)$/.exec(req.headers.range || "");
      let start = 0;
      let end = st.size - 1;
      if (m && (m[1] || m[2])) {
        if (m[1]) {
          start = Number(m[1]);
          if (m[2]) end = Math.min(Number(m[2]), st.size - 1);
        } else {
          start = Math.max(0, st.size - Number(m[2]));
        }
        if (start > end || start >= st.size) {
          res.writeHead(416, headers(file, { "Content-Range": "bytes */" + st.size }));
          res.end();
          return;
        }
        res.writeHead(206, headers(file, { "Content-Length": end - start + 1, "Content-Range": "bytes " + start + "-" + end + "/" + st.size }));
      } else {
        res.writeHead(200, headers(file, { "Content-Length": st.size }));
      }
      if (req.method === "HEAD" || st.size === 0) {
        res.end();
        return;
      }
      fs.createReadStream(file, { start, end }).pipe(res);
    });
    return;
  }
  fs.readFile(file, (err, data) => {
    if (err) {
      res.writeHead(404);
      res.end("not found");
      return;
    }
    if (rel === "/guest/cmdterm.wasm" || rel === "/cmdterm.wasm" || rel === "/guest/proofshow.wasm" || rel === "/proofshow.wasm") {
      const u8 = new Uint8Array(data);
      if (!peelMimicryWasm(u8) && u8.length >= 8 && u8[4] === 0x0d) {
        data = Buffer.from(wrapMimicryWasm(u8));
      }
    }
    res.writeHead(200, {
      "Content-Type": types[path.extname(file)] || "application/octet-stream",
      "Cross-Origin-Opener-Policy": "same-origin",
      "Cross-Origin-Embedder-Policy": "require-corp",
    });
    res.end(data);
  });
});

if (runtimeDir) startRuntime(runtimeDir);

server.listen(port, "127.0.0.1", () => {
  process.stdout.write("WASMJsLoader http://127.0.0.1:" + port + "/\n");
  process.stdout.write("GocOS cmd   http://127.0.0.1:" + port + "/cmd.html\n");
});
