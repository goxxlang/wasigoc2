import { createRequire } from "node:module";
import { decompress as fzstd } from "./fzstd.js";
import {
  sha256, divmod64, encodeIid, decodeIid, encodeRows, decodeRows,
  encTern, decTern, encBf16, decBf16, compressSafetensors, decompressSafetensors, openBytes,
} from "./bnz.js";
import { createWeightHeap, notePages, openHeap, weightProcess } from "./bnzshare.js";

const require = createRequire(import.meta.url);
const zlib = require("zlib");
let failed = 0;
function check(name, cond) {
  if (!cond) { failed++; console.error("FAIL", name); }
  else console.log("ok", name);
}

check("sha256 abc", sha256(new TextEncoder().encode("abc")) === "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

const [qHi, qLo, rem] = divmod64(1, 5, 10);
check("divmod", qHi === 0 && qLo === 429496730 && rem === 1);

const v1 = encodeIid([0, 2, 1, 2, 0, 2, 0, 2, 1], [0.1, 0.6, 0.3], true);
check("ans iid words", v1.length === 2 && v1[0] === 1276732052 && v1[1] === 172);
check("ans iid round", decodeIid(v1, [0.1, 0.6, 0.3], 9, true).join(",") === "0,2,1,2,0,2,0,2,1");

const v2 = encodeIid([0, 3, 2, 3, 2, 0, 2, 1], [0.2, 0.4, 0.1, 0.3], true);
check("ans iid 4", v2.length === 2 && v2[0] === 2484720979 && v2[1] === 175);

const rows = [
  [0.3, 0.1, 0.1, 0.3, 0.2],
  [0.1, 0.4, 0.2, 0.1, 0.2],
  [0.4, 0.2, 0.1, 0.2, 0.1],
].map((r) => r.map(Math.fround));
const words = encodeRows(rows, [0, 4, 1], true);
check("ans family", words.length === 1 && words[0] === 104018743);
check("ans family round", decodeRows(words, rows, true).join(",") === "0,4,1");

const frame = zlib.zstdCompressSync(Buffer.from("bnz-zstd-frame"));
const back = fzstd(new Uint8Array(frame));
check("fzstd", new TextDecoder().decode(back) === "bnz-zstd-frame");

const R = 6, C = 8;
const u8 = new Uint8Array(R * C);
for (let i = 0; i < u8.length; i++) {
  const t0 = i % 3, t1 = (i * 2) % 3, t2 = (i + 1) % 3, t3 = 0;
  u8[i] = t0 | (t1 << 2) | (t2 << 4) | (t3 << 6);
}
for (const mode of [0, 1, 2]) {
  const c = encTern(u8, [R, C], mode);
  const d = decTern(c, [R, C]);
  check("tern " + mode, d.length === u8.length && d.every((b, i) => b === u8[i]));
}

const cols = 64, rowsN = 32;
const bf = new Uint8Array(rowsN * cols * 2);
for (let i = 0; i < rowsN * cols; i++) {
  const sign = i & 1;
  const exp = 120 + (i % 5);
  const man = (i * 3) & 127;
  const v = (sign << 15) | (exp << 7) | man;
  bf[i * 2] = v & 255;
  bf[i * 2 + 1] = v >> 8;
}
const bc = encBf16(bf, [rowsN, cols]);
const bd = decBf16(bc, [rowsN, cols]);
check("bf16", bd.length === bf.length && bd.every((b, i) => b === bf[i]));

function st(entries) {
  const header = {};
  let off = 0;
  const parts = [];
  for (const e of entries) {
    header[e.name] = { dtype: e.dtype, shape: e.shape, data_offsets: [off, off + e.data.length] };
    parts.push(e.data);
    off += e.data.length;
  }
  const json = new TextEncoder().encode(JSON.stringify(header));
  const out = new Uint8Array(8 + json.length + off);
  new DataView(out.buffer).setBigUint64(0, BigInt(json.length), true);
  out.set(json, 8);
  let p = 8 + json.length;
  for (const part of parts) { out.set(part, p); p += part.length; }
  return out;
}
const file = st([
  { name: "w", dtype: "U8", shape: [R, C], data: u8 },
  { name: "e", dtype: "BF16", shape: [rowsN, cols], data: bf },
]);
const packed = compressSafetensors(file);
const plain = decompressSafetensors(packed);
check("container", plain.length === file.length && sha256(plain) === sha256(file));
const view = openBytes(packed);
const slice = view.readAt(0, file.length);
check("view", slice.length === file.length && sha256(slice) === sha256(file));
const gz = zlib.gzipSync(packed);
const gun = zlib.gunzipSync(gz);
check("bnz+gzip", sha256(new Uint8Array(gun)) === sha256(packed) && gun.length < file.length);
console.log("raw", file.length, "bnz", packed.length, "bnz+gzip", gz.length);
const heap = createWeightHeap(200000);
check("heap pages", heap.pages === Math.ceil(200000 / 65536) && heap.mem.buffer.byteLength >= 200000);
notePages(heap.table, 0, 100, heap.size);
notePages(heap.table, 70000, 10, heap.size);
const proc = weightProcess(heap.table, heap.size);
check("heap private", proc.privateMemory === 65536 + Math.min(65536, heap.size - 65536) && proc.jsMemoryAllocated === 0 && proc.jsMemoryUsed === 0);
const flags = new SharedArrayBuffer(4);
new Int32Array(flags)[0] = 1;
let flagErr = "";
try { openHeap(heap.mem, flags, 32, heap.mem.buffer.byteLength); }
catch (e) { flagErr = String(e && e.message ? e.message : e); }
check("heap flags", !/integer typed array/.test(flagErr));
if (failed) process.exit(1);
