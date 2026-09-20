// xterm.js Command Prompt. Instantiates cmdterm.wasm in a worker via
// loader.js (wasigocvm guest). Win32/GocOS stay inside the module.
const term = new Terminal({
  cursorBlink: true,
  convertEol: true,
  fontFamily: 'Consolas, "Cascadia Mono", "Courier New", monospace',
  fontSize: 15,
  theme: {
    background: "#0c0c0c",
    foreground: "#cccccc",
    cursor: "#ffffff",
    selectionBackground: "#264f78",
  },
});
term.open(document.getElementById("term"));
term.focus();

function fail(msg) {
  term.write("\r\n\x1b[31m" + msg + "\x1b[0m\r\n");
}

if (typeof SharedArrayBuffer !== "function") {
  fail("SharedArrayBuffer missing — open this page over WASMJsLoader serve.mjs (COOP/COEP), not file://");
  throw new Error("no SAB");
}

const sab = new SharedArrayBuffer(65536);
const i32 = new Int32Array(sab);
const u8 = new Uint8Array(sab);
const worker = new Worker("./cmd-worker.js", { type: "module" });

worker.onmessage = (e) => {
  const msg = e.data || {};
  if (msg.type === "out") term.write(msg.text);
  if (msg.type === "err") term.write("\x1b[31m" + msg.text + "\x1b[0m");
  if (msg.type === "exit") term.write("\r\n");
};

worker.onerror = (e) => fail(e.message || "worker error");

let line = "";
term.onData((ch) => {
  if (ch === "\r") {
    term.write("\r\n");
    const enc = new TextEncoder().encode(line + "\n");
    line = "";
    if (enc.length > 65536 - 16) return;
    u8.set(enc, 16);
    Atomics.store(i32, 1, enc.length);
    Atomics.store(i32, 0, 1);
    Atomics.notify(i32, 0);
    return;
  }
  if (ch === "\u007f") {
    if (line.length) {
      line = line.slice(0, -1);
      term.write("\b \b");
    }
    return;
  }
  if (ch === "\u0003") {
    term.write("^C\r\n");
    line = "";
    return;
  }
  if (ch.length === 1 && ch >= " ") {
    line += ch;
    term.write(ch);
  }
});

const wasmUrl = new URL("./guest/cmdterm.wasm", import.meta.url);
term.write("loading " + wasmUrl.pathname + " …\r\n");

fetch(wasmUrl)
  .then((r) => {
    if (!r.ok) throw new Error("fetch " + wasmUrl.pathname + " " + r.status);
    return r.arrayBuffer();
  })
  .then((bytes) => {
    worker.postMessage({ type: "boot", bytes, sab }, [bytes]);
  })
  .catch((err) => fail(err && err.message ? err.message : String(err)));
