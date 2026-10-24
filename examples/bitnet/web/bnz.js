// BNZ2, the BitNet b1.58 safetensors codec (ternary U8 + BF16), in JS.
// The ANS coder is constriction's default stack coder: 24-bit probabilities,
// 32-bit words, 64-bit state, Categorical(perfect=False). Zstd frames wrap
// the side tables. Gzip is not part of the container; the share layer gzips
// each chunk on the wire.
import { decompress as fzstdDecompress } from "./fzstd.js";

const PREC = 24;
const PREC_MASK = (1 << PREC) - 1;
const TOTAL = 1 << PREC;
const TWO32 = 0x100000000;

function nodeZlib() {
  if (typeof process === "undefined" || !process.versions || !process.versions.node) return null;
  if (typeof process.getBuiltinModule !== "function") return null;
  return process.getBuiltinModule("zlib");
}

function asBytes(b) {
  return b.byteOffset === 0 && b.byteLength === b.buffer.byteLength ? b : b.slice();
}

export function zstdCompress(bytes) {
  const z = nodeZlib();
  if (!z) throw new Error("bnz: zstd encode runs in node");
  return new Uint8Array(z.zstdCompressSync(asBytes(bytes)));
}

export function zstdDecompress(bytes) {
  const src = asBytes(bytes);
  const z = nodeZlib();
  if (z) return new Uint8Array(z.zstdDecompressSync(src));
  return fzstdDecompress(src);
}

// --- sha256 (sync; the guest reads weights on the worker thread) ---
const K256 = new Uint32Array([
  0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
  0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
  0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
  0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
  0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
  0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
  0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
  0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
]);

function rotr(x, n) { return (x >>> n) | (x << (32 - n)); }

export function sha256(data) {
  const bytes = data instanceof Uint8Array ? data : new Uint8Array(data);
  const bitLen = bytes.length * 8;
  const withPad = ((bytes.length + 9 + 63) >> 6) << 6;
  const buf = new Uint8Array(withPad);
  buf.set(bytes);
  buf[bytes.length] = 0x80;
  const dv = new DataView(buf.buffer);
  const hi = Math.floor(bitLen / TWO32);
  dv.setUint32(withPad - 8, hi, false);
  dv.setUint32(withPad - 4, bitLen >>> 0, false);
  let h0 = 0x6a09e667, h1 = 0xbb67ae85, h2 = 0x3c6ef372, h3 = 0xa54ff53a;
  let h4 = 0x510e527f, h5 = 0x9b05688c, h6 = 0x1f83d9ab, h7 = 0x5be0cd19;
  const w = new Uint32Array(64);
  for (let i = 0; i < withPad; i += 64) {
    for (let t = 0; t < 16; t++) w[t] = dv.getUint32(i + t * 4, false);
    for (let t = 16; t < 64; t++) {
      const x = w[t - 15], y = w[t - 2];
      const s0 = rotr(x, 7) ^ rotr(x, 18) ^ (x >>> 3);
      const s1 = rotr(y, 17) ^ rotr(y, 19) ^ (y >>> 10);
      w[t] = (w[t - 16] + s0 + w[t - 7] + s1) >>> 0;
    }
    let a = h0, b = h1, c = h2, d = h3, e = h4, f = h5, g = h6, h = h7;
    for (let t = 0; t < 64; t++) {
      const S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
      const ch = (e & f) ^ (~e & g);
      const t1 = (h + S1 + ch + K256[t] + w[t]) >>> 0;
      const S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
      const maj = (a & b) ^ (a & c) ^ (b & c);
      const t2 = (S0 + maj) >>> 0;
      h = g; g = f; f = e; e = (d + t1) >>> 0; d = c; c = b; b = a; a = (t1 + t2) >>> 0;
    }
    h0 = (h0 + a) >>> 0; h1 = (h1 + b) >>> 0; h2 = (h2 + c) >>> 0; h3 = (h3 + d) >>> 0;
    h4 = (h4 + e) >>> 0; h5 = (h5 + f) >>> 0; h6 = (h6 + g) >>> 0; h7 = (h7 + h) >>> 0;
  }
  return [h0, h1, h2, h3, h4, h5, h6, h7].map((x) => x.toString(16).padStart(8, "0")).join("");
}

// --- fast categorical CDF, perfect=False ---
// useF32 matches a float32 numpy array. bnz.py's count models are float64.
export function cdfFast(probs, useF32) {
  const n = probs.length;
  const num = (x) => (useF32 ? Math.fround(x) : x);
  const free = TOTAL - n;
  let norm = num(0);
  for (let i = 0; i < n; i++) norm = num(norm + num(probs[i]));
  const scale = num(free / norm);
  const cdf = new Uint32Array(n + 1);
  let c = num(0);
  for (let i = 0; i < n; i++) {
    cdf[i] = (Math.trunc(num(c * scale)) + i) >>> 0;
    c = num(c + num(probs[i]));
  }
  cdf[n] = TOTAL;
  return cdf;
}

// 64-bit / 32-bit. prob is below 2^24, so the intermediate product fits in a
// safe integer. Returns [qHi, qLo, rem].
export function divmod64(hi, lo, d) {
  hi >>>= 0; lo >>>= 0; d >>>= 0;
  if (hi === 0) return [0, Math.floor(lo / d) >>> 0, lo % d];
  const qHi = Math.floor(hi / d);
  const rHi = hi % d;
  const q0 = Math.floor(TWO32 / d);
  const two32mod = TWO32 % d;
  const mid = rHi * two32mod + lo;
  const qRest = rHi * q0 + Math.floor(mid / d);
  const rem = mid % d;
  const qHiOut = (qHi + Math.floor(qRest / TWO32)) >>> 0;
  const qLoOut = (qRest % TWO32) >>> 0;
  return [qHiOut, qLoOut, rem];
}

function shl24or(qHi, qLo, quantile) {
  const lo = ((qLo << 24) | quantile) >>> 0;
  const hi = ((qHi << 24) | (qLo >>> 8)) >>> 0;
  return [hi, lo];
}

function mul32(a, b) {
  const aLo = a & 0xffff, aHi = a >>> 16;
  const bLo = b & 0xffff, bHi = b >>> 16;
  const p0 = aLo * bLo;
  const p1 = aLo * bHi;
  const p2 = aHi * bLo;
  const p3 = aHi * bHi;
  const mid = (p0 >>> 16) + (p1 & 0xffff) + (p2 & 0xffff);
  const lo = ((p0 & 0xffff) | ((mid & 0xffff) << 16)) >>> 0;
  const hi = (p3 + (p1 >>> 16) + (p2 >>> 16) + (mid >>> 16)) >>> 0;
  return [hi, lo];
}

export class AnsCoder {
  constructor(words) {
    this.bulk = words ? words.slice() : [];
    this.hi = 0;
    this.lo = 0;
    if (words && words.length) this.#init();
  }

  #init() {
    const first = this.bulk.pop();
    if ((first >>> 0) === 0) throw new Error("bnz: trailing zero word");
    this.lo = first >>> 0;
    this.hi = 0;
    while (this.bulk.length) {
      const word = this.bulk.pop() >>> 0;
      this.hi = this.lo;
      this.lo = word;
      if (this.hi !== 0) break;
    }
  }

  encodeSymbol(left, prob) {
    prob >>>= 0;
    left >>>= 0;
    if ((this.hi >>> 8) >= prob) {
      this.bulk.push(this.lo >>> 0);
      this.lo = this.hi >>> 0;
      this.hi = 0;
    }
    const [qHi, qLo, rem] = divmod64(this.hi, this.lo, prob);
    const st = shl24or(qHi, qLo, (left + rem) >>> 0);
    this.hi = st[0];
    this.lo = st[1];
  }

  decodeSymbol(cdf) {
    const q = this.lo & PREC_MASK;
    let lo = 0, hi = cdf.length - 1;
    while (lo < hi) {
      const mid = (lo + hi) >> 1;
      if (cdf[mid] <= q) lo = mid + 1;
      else hi = mid;
    }
    const symbol = lo - 1;
    const left = cdf[symbol];
    const prob = cdf[lo] - left;
    this.#mulAdd(prob, q - left);
    return symbol;
  }

  #mulAdd(prob, add) {
    const sLo = ((this.lo >>> 24) | ((this.hi & 0xffffff) << 8)) >>> 0;
    const sHi = this.hi >>> 24;
    const [pHi, pLo] = mul32(sLo, prob);
    const sumLo = pLo + add;
    const rLo = sumLo >>> 0;
    const carry = sumLo >= TWO32 ? 1 : 0;
    this.hi = (pHi + sHi * prob + carry) >>> 0;
    this.lo = rLo;
    if (this.hi === 0 && this.bulk.length) {
      this.hi = this.lo;
      this.lo = this.bulk.pop() >>> 0;
    }
  }

  getCompressed() {
    const extra = this.hi === 0 ? (this.lo === 0 ? [] : [this.lo >>> 0]) : [this.lo >>> 0, this.hi >>> 0];
    return this.bulk.concat(extra);
  }
}

export function encodeIid(symbols, probs, useF32) {
  const cdf = cdfFast(probs, useF32);
  const enc = new AnsCoder();
  for (let i = symbols.length - 1; i >= 0; i--) {
    const s = symbols[i];
    enc.encodeSymbol(cdf[s], cdf[s + 1] - cdf[s]);
  }
  return enc.getCompressed();
}

export function decodeIid(words, probs, n, useF32) {
  const cdf = cdfFast(probs, useF32);
  const dec = new AnsCoder(words);
  const out = new Array(n);
  for (let i = 0; i < n; i++) out[i] = dec.decodeSymbol(cdf);
  return out;
}

export function encodeRows(rows, symbols, useF32) {
  const enc = new AnsCoder();
  for (let i = symbols.length - 1; i >= 0; i--) {
    const cdf = cdfFast(rows[i], !!useF32);
    const s = symbols[i];
    enc.encodeSymbol(cdf[s], cdf[s + 1] - cdf[s]);
  }
  return enc.getCompressed();
}

export function decodeRows(words, rows, useF32) {
  const dec = new AnsCoder(words);
  const out = new Array(rows.length);
  for (let i = 0; i < rows.length; i++) out[i] = dec.decodeSymbol(cdfFast(rows[i], !!useF32));
  return out;
}

function wordsToBytes(words) {
  const o = new Uint8Array(words.length * 4);
  const dv = new DataView(o.buffer);
  for (let i = 0; i < words.length; i++) dv.setUint32(i * 4, words[i] >>> 0, true);
  return o;
}

function bytesToWords(b) {
  const n = b.length >> 2;
  const words = new Array(n);
  const dv = new DataView(b.buffer, b.byteOffset, b.byteLength);
  for (let i = 0; i < n; i++) words[i] = dv.getUint32(i * 4, true);
  return words;
}

function pk(parts) {
  let n = 0;
  for (const p of parts) n += 8 + p.length;
  const o = new Uint8Array(n);
  const dv = new DataView(o.buffer);
  let i = 0;
  for (const p of parts) {
    dv.setBigUint64(i, BigInt(p.length), true);
    i += 8;
    o.set(p, i);
    i += p.length;
  }
  return o;
}

function upk(b) {
  const dv = new DataView(b.buffer, b.byteOffset, b.byteLength);
  const out = [];
  let i = 0;
  while (i + 8 <= b.length) {
    const n = Number(dv.getBigUint64(i, true));
    i += 8;
    out.push(b.subarray(i, i + n));
    i += n;
  }
  return out;
}

function u32bytes(arr) {
  const o = new Uint8Array(arr.length * 4);
  const dv = new DataView(o.buffer);
  for (let i = 0; i < arr.length; i++) dv.setUint32(i * 4, arr[i] >>> 0, true);
  return o;
}

function bytesU32(b) {
  const n = b.length >> 2;
  const a = new Uint32Array(n);
  const dv = new DataView(b.buffer, b.byteOffset, b.byteLength);
  for (let i = 0; i < n; i++) a[i] = dv.getUint32(i * 4, true);
  return a;
}

// --- ternary U8: four 2-bit codes per byte, values 0..2 ---
function splitTrits(raw, R, C) {
  const N = 4 * R;
  const t = new Int32Array(N * C);
  for (let r = 0; r < R; r++) {
    for (let c = 0; c < C; c++) {
      const a = raw[r * C + c];
      for (let s = 0; s < 4; s++) {
        const v = (a >>> (2 * s)) & 3;
        if (v === 3) return null;
        t[(s * R + r) * C + c] = v;
      }
    }
  }
  return t;
}

function tritCounts(t, N, C) {
  const rc = new Uint32Array(N * 3);
  const cc = new Uint32Array(C * 3);
  const total = N * C;
  for (let i = 0; i < total; i++) {
    const v = t[i];
    const row = (i / C) | 0;
    const col = i - row * C;
    rc[row * 3 + v]++;
    cc[col * 3 + v]++;
  }
  const gc = new Float64Array(3);
  for (let r = 0; r < N; r++) for (let k = 0; k < 3; k++) gc[k] += rc[r * 3 + k];
  return { rc, cc, gc };
}

function probsAt(mode, rc, cc, gc, row, col) {
  const p = [0, 0, 0];
  for (let k = 0; k < 3; k++) {
    if (mode === 0) p[k] = rc[row * 3 + k] + 0.5;
    else if (mode === 1) p[k] = cc[col * 3 + k] + 0.5;
    else p[k] = (rc[row * 3 + k] + 0.5) * (cc[col * 3 + k] + 0.5) / (gc[k] + 0.5);
  }
  return p;
}

function cdfAt(mode, rc, cc, gc, row, col, cache) {
  if (mode === 0) {
    let hit = cache.row[row];
    if (!hit) hit = cache.row[row] = cdfFast(probsAt(0, rc, cc, gc, row, 0), false);
    return hit;
  }
  if (mode === 1) {
    let hit = cache.col[col];
    if (!hit) hit = cache.col[col] = cdfFast(probsAt(1, rc, cc, gc, 0, col), false);
    return hit;
  }
  return cdfFast(probsAt(2, rc, cc, gc, row, col), false);
}

export function encTern(raw, shape, mode) {
  const R = shape[0], C = shape[1];
  const t = splitTrits(raw, R, C);
  if (!t) return null;
  const N = 4 * R;
  const { rc, cc, gc } = tritCounts(t, N, C);
  const enc = new AnsCoder();
  const cache = { row: new Array(N), col: new Array(C) };
  const total = N * C;
  for (let i = total - 1; i >= 0; i--) {
    const row = (i / C) | 0;
    const col = i - row * C;
    const cdf = cdfAt(mode, rc, cc, gc, row, col, cache);
    const s = t[i];
    enc.encodeSymbol(cdf[s], cdf[s + 1] - cdf[s]);
  }
  const tab = u32bytes(rc);
  const both = new Uint8Array(tab.length + cc.length * 4);
  both.set(tab, 0);
  both.set(u32bytes(cc), tab.length);
  return pk([new Uint8Array([mode]), zstdCompress(both), wordsToBytes(enc.getCompressed())]);
}

export function decTern(comp, shape) {
  const R = shape[0], C = shape[1];
  const N = 4 * R;
  const parts = upk(comp);
  const mode = parts[0][0];
  const tb = bytesU32(zstdDecompress(parts[1]));
  const rc = new Float64Array(N * 3);
  const cc = new Float64Array(C * 3);
  for (let i = 0; i < N * 3; i++) rc[i] = tb[i];
  for (let i = 0; i < C * 3; i++) cc[i] = tb[N * 3 + i];
  const gc = new Float64Array(3);
  for (let r = 0; r < N; r++) for (let k = 0; k < 3; k++) gc[k] += rc[r * 3 + k];
  const rcU = rc, ccU = cc;
  const dec = new AnsCoder(bytesToWords(parts[2]));
  const cache = { row: new Array(N), col: new Array(C) };
  const out = new Uint8Array(N * C);
  const total = N * C;
  for (let i = 0; i < total; i++) {
    const row = (i / C) | 0;
    const col = i - row * C;
    out[i] = dec.decodeSymbol(cdfAt(mode, rcU, ccU, gc, row, col, cache));
  }
  const raw = new Uint8Array(R * C);
  for (let r = 0; r < R; r++) {
    for (let c = 0; c < C; c++) {
      let a = 0;
      for (let s = 0; s < 4; s++) a |= out[(s * R + r) * C + c] << (2 * s);
      raw[r * C + c] = a;
    }
  }
  return raw;
}

// --- bf16: sign bits, per-column exponents, per-exponent mantissas ---
function readU16(raw) {
  const n = raw.length >> 1;
  const v = new Uint16Array(n);
  for (let i = 0; i < n; i++) v[i] = raw[i * 2] | (raw[i * 2 + 1] << 8);
  return v;
}

function packBits(bits) {
  const o = new Uint8Array((bits.length + 7) >> 3);
  for (let i = 0; i < bits.length; i++) if (bits[i]) o[i >> 3] |= 1 << (7 - (i & 7));
  return o;
}

function unpackBits(packed, n) {
  const o = new Uint8Array(n);
  for (let i = 0; i < n; i++) o[i] = (packed[i >> 3] >> (7 - (i & 7))) & 1;
  return o;
}

function nonzeroCount(counts) {
  let nz = 0, only = 0;
  for (let i = 0; i < counts.length; i++) if (counts[i]) { nz++; only = i; }
  return [nz, only];
}

function encodeCounts(enc, syms, counts) {
  if (syms.length === 0) return;
  const [nz] = nonzeroCount(counts);
  if (nz <= 1) return;
  let sum = 0;
  for (let i = 0; i < counts.length; i++) sum += counts[i];
  const p = new Float64Array(counts.length);
  for (let i = 0; i < counts.length; i++) p[i] = counts[i] / sum;
  const cdf = cdfFast(p, false);
  for (let i = syms.length - 1; i >= 0; i--) {
    const s = syms[i];
    enc.encodeSymbol(cdf[s], cdf[s + 1] - cdf[s]);
  }
}

function decodeCounts(dec, n, counts) {
  const out = new Int32Array(n);
  if (n === 0) return out;
  const [nz, only] = nonzeroCount(counts);
  if (nz <= 1) { out.fill(only); return out; }
  let sum = 0;
  for (let i = 0; i < counts.length; i++) sum += counts[i];
  const p = new Float64Array(counts.length);
  for (let i = 0; i < counts.length; i++) p[i] = counts[i] / sum;
  const cdf = cdfFast(p, false);
  for (let i = 0; i < n; i++) out[i] = dec.decodeSymbol(cdf);
  return out;
}

export function encBf16(raw, shape) {
  const v = readU16(raw);
  const C = shape.length === 2 ? shape[shape.length - 1] : 1;
  const n = v.length;
  const Rr = n / C;
  let emin = 255, emax = 0;
  const ex = new Int32Array(n);
  const man = new Int32Array(n);
  const signBits = new Uint8Array(n);
  for (let i = 0; i < n; i++) {
    const x = v[i];
    const e = (x >> 7) & 255;
    if (e < emin) emin = e;
    if (e > emax) emax = e;
    ex[i] = e;
    man[i] = x & 127;
    signBits[i] = x >> 15;
  }
  const K = emax - emin + 1;
  const exf = new Int32Array(n);
  for (let i = 0; i < n; i++) exf[i] = ex[i] - emin;
  const ecnt = new Uint32Array(C * K);
  for (let r = 0; r < Rr; r++) {
    for (let c = 0; c < C; c++) ecnt[c * K + exf[r * C + c]]++;
  }
  const mcnt = new Uint32Array(K * 128);
  for (let i = 0; i < n; i++) mcnt[exf[i] * 128 + man[i]]++;
  const groups = Array.from({ length: K }, () => []);
  for (let i = 0; i < n; i++) groups[exf[i]].push(man[i]);
  const enc = new AnsCoder();
  for (let k = K - 1; k >= 0; k--) encodeCounts(enc, groups[k], mcnt.subarray(k * 128, (k + 1) * 128));
  for (let c = C - 1; c >= 0; c--) {
    const col = [];
    for (let r = 0; r < Rr; r++) col.push(exf[r * C + c]);
    encodeCounts(enc, col, ecnt.subarray(c * K, (c + 1) * K));
  }
  const hdr = new Uint8Array(12);
  const dv = new DataView(hdr.buffer);
  dv.setInt32(0, emin, true);
  dv.setInt32(4, K, true);
  dv.setUint32(8, C, true);
  const tab = new Uint8Array((C * K + K * 128) * 4);
  tab.set(u32bytes(ecnt), 0);
  tab.set(u32bytes(mcnt), C * K * 4);
  return pk([hdr, zstdCompress(tab), zstdCompress(packBits(signBits)), wordsToBytes(enc.getCompressed())]);
}

export function decBf16(comp, shape) {
  const parts = upk(comp);
  const hv = new DataView(parts[0].buffer, parts[0].byteOffset, parts[0].byteLength);
  const emin = hv.getInt32(0, true);
  const K = hv.getInt32(4, true);
  const C = hv.getUint32(8, true);
  const n = shape.reduce((a, b) => a * b, 1);
  const Rr = n / C;
  const tb = bytesU32(zstdDecompress(parts[1]));
  const ecnt = tb.subarray(0, C * K);
  const mcnt = tb.subarray(C * K);
  const dec = new AnsCoder(bytesToWords(parts[3]));
  const exf = new Int32Array(n);
  for (let c = 0; c < C; c++) {
    const col = decodeCounts(dec, Rr, ecnt.subarray(c * K, (c + 1) * K));
    for (let r = 0; r < Rr; r++) exf[r * C + c] = col[r];
  }
  const man = new Int32Array(n);
  for (let k = 0; k < K; k++) {
    const idx = [];
    for (let i = 0; i < n; i++) if (exf[i] === k) idx.push(i);
    const got = decodeCounts(dec, idx.length, mcnt.subarray(k * 128, (k + 1) * 128));
    for (let j = 0; j < idx.length; j++) man[idx[j]] = got[j];
  }
  const sign = unpackBits(zstdDecompress(parts[2]), n);
  const o = new Uint8Array(n * 2);
  for (let i = 0; i < n; i++) {
    const v = (sign[i] << 15) | ((exf[i] + emin) << 7) | man[i];
    o[i * 2] = v & 255;
    o[i * 2 + 1] = v >> 8;
  }
  return o;
}

// --- container ---
export const M_RAW = 0, M_ZSTD = 1, M_TERN = 2, M_BF16 = 3;
const EMB_CHUNK_BYTES = 8192 * 2560 * 2;

function ownBytes(b) {
  return b.buffer instanceof SharedArrayBuffer ? new Uint8Array(b) : b;
}
function textOf(b) { return new TextDecoder().decode(ownBytes(b)); }

export function segmentsOf(data) {
  const dv = new DataView(data.buffer, data.byteOffset, data.byteLength);
  const n = Number(dv.getBigUint64(0, true));
  const header = JSON.parse(textOf(data.subarray(8, 8 + n)));
  const base = 8 + n;
  const segs = [{ name: "header", dtype: null, shape: null, start: 0, end: base }];
  const items = [];
  for (const k of Object.keys(header)) {
    if (k === "__metadata__") continue;
    items.push([header[k].data_offsets[0], k, header[k]]);
  }
  items.sort((a, b) => a[0] - b[0]);
  let pos = base;
  for (const [, k, v] of items) {
    const s = base + v.data_offsets[0];
    const e = base + v.data_offsets[1];
    if (s > pos) segs.push({ name: "gap", dtype: null, shape: null, start: pos, end: s });
    if (v.dtype === "BF16" && v.shape.length === 2 && e - s > EMB_CHUNK_BYTES) {
      const rb = 2 * v.shape[1];
      const rows = Math.max(1, Math.floor(EMB_CHUNK_BYTES / rb));
      for (let r0 = 0; r0 < v.shape[0]; r0 += rows) {
        const r1 = Math.min(v.shape[0], r0 + rows);
        segs.push({ name: k + "[" + r0 + ":" + r1 + "]", dtype: "BF16", shape: [r1 - r0, v.shape[1]], start: s + r0 * rb, end: s + r1 * rb });
      }
    } else {
      segs.push({ name: k, dtype: v.dtype, shape: v.shape, start: s, end: e });
    }
    pos = e;
  }
  if (pos < data.length) segs.push({ name: "tail", dtype: null, shape: null, start: pos, end: data.length });
  return segs;
}

function pick(raw, dtype, shape) {
  const cands = [[M_RAW, raw]];
  if (raw.length > 64) cands.push([M_ZSTD, zstdCompress(raw)]);
  if (dtype === "U8" && shape && shape.length === 2) {
    for (const mode of [0, 1, 2]) {
      const c = encTern(raw, shape, mode);
      if (c) cands.push([M_TERN, c]);
    }
  }
  if (dtype === "BF16" && raw.length >= 4096) cands.push([M_BF16, encBf16(raw, shape)]);
  let best = cands[0];
  for (const c of cands) if (c[1].length < best[1].length) best = c;
  return best;
}

export function compressSafetensors(data) {
  const segs = segmentsOf(data);
  const recs = [];
  for (const seg of segs) {
    const raw = data.subarray(seg.start, seg.end);
    if (typeof process !== "undefined" && process.stderr) {
      process.stderr.write("seg " + (recs.length + 1) + "/" + segs.length + " " + seg.name + " " + raw.length + "\n");
    }
    const [method, comp] = pick(raw, seg.dtype, seg.shape);
    recs.push({
      name: seg.name,
      method,
      raw_off: seg.start,
      raw_len: raw.length,
      shape: method === M_TERN || method === M_BF16 ? seg.shape : null,
      sha256: sha256(raw),
      comp,
    });
  }
  let off = 0;
  const idx = [];
  for (const r of recs) {
    idx.push({ name: r.name, method: r.method, raw_off: r.raw_off, raw_len: r.raw_len, off, len: r.comp.length, shape: r.shape, sha256: r.sha256 });
    off += r.comp.length;
  }
  const ij = new TextEncoder().encode(JSON.stringify({ orig_size: data.length, sha256: sha256(data), segments: idx }));
  const out = new Uint8Array(4 + 8 + ij.length + off);
  out.set([0x42, 0x4e, 0x5a, 0x32], 0);
  new DataView(out.buffer).setBigUint64(4, BigInt(ij.length), true);
  out.set(ij, 12);
  let p = 12 + ij.length;
  for (const r of recs) { out.set(r.comp, p); p += r.comp.length; }
  return out;
}

export function parseBnz(bnz) {
  if (bnz.length < 12 || bnz[0] !== 0x42 || bnz[1] !== 0x4e || bnz[2] !== 0x5a || bnz[3] !== 0x32) {
    throw new Error("bnz: not a BNZ2 file");
  }
  const n = Number(new DataView(bnz.buffer, bnz.byteOffset, bnz.byteLength).getBigUint64(4, true));
  const index = JSON.parse(textOf(bnz.subarray(12, 12 + n)));
  return { index, base: 12 + n };
}

export function decodeSeg(seg, comp) {
  let raw;
  if (seg.method === M_RAW) raw = asBytes(comp);
  else if (seg.method === M_ZSTD) raw = zstdDecompress(comp);
  else if (seg.method === M_TERN) raw = decTern(comp, seg.shape);
  else if (seg.method === M_BF16) raw = decBf16(comp, seg.shape);
  else throw new Error("bnz: bad method");
  if (raw.length !== seg.raw_len) throw new Error("bnz: " + seg.name + " length");
  if (sha256(raw) !== seg.sha256) throw new Error("bnz: " + seg.name + " hash");
  return raw;
}

export function decompressSafetensors(bnz) {
  const { index, base } = parseBnz(bnz);
  const out = new Uint8Array(index.orig_size);
  for (const seg of index.segments) {
    const raw = decodeSeg(seg, bnz.subarray(base + seg.off, base + seg.off + seg.len));
    out.set(raw, seg.raw_off);
  }
  if (sha256(out) !== index.sha256) throw new Error("bnz: file hash");
  return out;
}

// A safetensors-sized view. readAt decodes one BNZ segment at a time.
export function openBytes(bnz, ensure) {
  if (ensure) {
    ensure(0, Math.min(12, bnz.length));
    const jsonLen = Number(new DataView(bnz.buffer, bnz.byteOffset, 12).getBigUint64(4, true));
    ensure(0, Math.min(bnz.length, 12 + jsonLen));
  }
  const { index, base } = parseBnz(bnz);
  const segs = index.segments;
  let cache = null;
  function find(pos) {
    let lo = 0, hi = segs.length - 1, ans = 0;
    while (lo <= hi) {
      const mid = (lo + hi) >> 1;
      if (segs[mid].raw_off <= pos) { ans = mid; lo = mid + 1; }
      else hi = mid - 1;
    }
    const seg = segs[ans];
    if (pos < seg.raw_off || pos >= seg.raw_off + seg.raw_len) throw new Error("bnz: hole");
    return seg;
  }
  return {
    size: index.orig_size,
    readAt(pos, n) {
      const out = new Uint8Array(n);
      let filled = 0;
      while (filled < n) {
        const seg = find(pos + filled);
        if (!cache || cache.seg !== seg) {
          if (ensure) ensure(base + seg.off, seg.len);
          cache = { seg, raw: decodeSeg(seg, ownBytes(bnz.subarray(base + seg.off, base + seg.off + seg.len))) };
        }
        const inner = pos + filled - seg.raw_off;
        const take = Math.min(n - filled, seg.raw_len - inner);
        if (take <= 0) throw new Error("bnz: hole");
        out.set(cache.raw.subarray(inner, inner + take), filled);
        filled += take;
      }
      return out;
    },
  };
}
