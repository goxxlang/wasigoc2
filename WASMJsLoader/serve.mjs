#!/usr/bin/env node
// Static hop for WASMJsLoader. COOP/COEP so SharedArrayBuffer works
// if the guest was built with threads. The guest Calls loader.js;
// this process only serves bytes.
import http from "node:http";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
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
};

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

const server = http.createServer((req, res) => {
  const url = new URL(req.url || "/", "http://127.0.0.1");
  let rel = decodeURIComponent(url.pathname);
  if (rel === "/") rel = "/index.html";
  let file;
  if (rel === "/guest/cmdterm.wasm" || rel === "/cmdterm.wasm") {
    file = guestWasm();
  } else if (rel === "/guest/proofshow.wasm" || rel === "/proofshow.wasm") {
    file = proofWasm();
  } else {
    file = path.normalize(path.join(root, rel));
    if (!file.startsWith(root)) {
      res.writeHead(403);
      res.end();
      return;
    }
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

server.listen(port, "127.0.0.1", () => {
  process.stdout.write("WASMJsLoader http://127.0.0.1:" + port + "/\n");
  process.stdout.write("GocOS cmd   http://127.0.0.1:" + port + "/cmd.html\n");
});
