// On-device embedder. No outside embedding model.
// A fixed ternary LUT, packed like the WGSL weight codes, then one discrete
// exterior step on the token path: coboundary, Laplacian smooth, mean.
// The LUT is stored as a BNZ2 module so new weights use the same codec.

import { decompressSafetensors, sha256 } from "./bnz.js";

export const DIM = 32;
export const BUCKETS = 256;

function mix(n) {
  n >>>= 0;
  n = Math.imul(n ^ (n >>> 16), 0x7feb352d) >>> 0;
  n = Math.imul(n ^ (n >>> 15), 0x846ca68b) >>> 0;
  return (n ^ (n >>> 16)) >>> 0;
}

export function buildLut() {
  const words = new Uint32Array(BUCKETS * (DIM / 16));
  for (let b = 0; b < BUCKETS; b++) {
    for (let d = 0; d < DIM; d++) {
      const code = mix(Math.imul(b, 131) + Math.imul(d, 17) + 0x9e3779b9) % 3;
      const idx = b * DIM + d;
      words[(idx / 16) | 0] |= code << ((idx % 16) * 2);
    }
  }
  return words;
}

export function codeAt(words, bucket, d) {
  const idx = bucket * DIM + d;
  const w = words[(idx / 16) | 0];
  return ((w >>> ((idx % 16) * 2)) & 3) - 1;
}

export function tokensOf(text) {
  const words = String(text || "").toLowerCase().match(/[a-z0-9]+/g);
  if (!words || !words.length) return [0];
  return words.slice(0, 64).map((word) => {
    let h = 0x811c9dc5;
    for (let i = 0; i < word.length; i++) {
      h ^= word.charCodeAt(i);
      h = Math.imul(h, 0x01000193);
    }
    return (h >>> 0) % BUCKETS;
  });
}

export function embedRaw(ids, lut) {
  const n = ids.length || 1;
  const acc = new Float32Array(DIM);
  for (let d = 0; d < DIM; d++) {
    let sum = 0;
    for (let i = 0; i < ids.length; i++) {
      const v = codeAt(lut, ids[i], d);
      let lv = 0;
      if (ids.length > 1) {
        if (i === 0) lv = v - codeAt(lut, ids[1], d);
        else if (i + 1 === ids.length) lv = v - codeAt(lut, ids[i - 1], d);
        else lv = 2 * v - codeAt(lut, ids[i - 1], d) - codeAt(lut, ids[i + 1], d);
      }
      sum += v - 0.5 * lv;
    }
    acc[d] = sum / n;
  }
  return acc;
}

export function normalize(acc) {
  let norm = 0;
  for (let i = 0; i < acc.length; i++) norm += acc[i] * acc[i];
  norm = Math.sqrt(norm) || 1;
  const out = new Float32Array(acc.length);
  for (let i = 0; i < acc.length; i++) out[i] = acc[i] / norm;
  return out;
}

export function embedTokens(ids, lut) {
  return normalize(embedRaw(ids, lut));
}

export function embedText(text, lut) {
  return embedTokens(tokensOf(text), lut);
}

export function cosine(a, b) {
  let dot = 0, na = 0, nb = 0;
  const n = Math.min(a.length, b.length);
  for (let i = 0; i < n; i++) {
    dot += a[i] * b[i];
    na += a[i] * a[i];
    nb += b[i] * b[i];
  }
  return dot / Math.sqrt(na * nb + 1e-12);
}

function safetensorU8(name, bytes, shape) {
  const header = {};
  header[name] = { dtype: "U8", shape, data_offsets: [0, bytes.length] };
  const json = new TextEncoder().encode(JSON.stringify(header));
  const out = new Uint8Array(8 + json.length + bytes.length);
  new DataView(out.buffer).setBigUint64(0, BigInt(json.length), true);
  out.set(json, 8);
  out.set(bytes, 8 + json.length);
  return out;
}

export function lutBytes(lut) {
  return new Uint8Array(lut.buffer, lut.byteOffset, lut.byteLength);
}

function packRawModule(raw) {
  const body = {
    orig_size: raw.length,
    sha256: sha256(raw),
    segments: [{
      name: "embed.lut",
      method: 0,
      raw_off: 0,
      raw_len: raw.length,
      off: 0,
      len: raw.length,
      shape: null,
      sha256: sha256(raw),
    }],
  };
  const ij = new TextEncoder().encode(JSON.stringify(body));
  const out = new Uint8Array(12 + ij.length + raw.length);
  out.set([0x42, 0x4e, 0x5a, 0x32], 0);
  new DataView(out.buffer).setBigUint64(4, BigInt(ij.length), true);
  out.set(ij, 12);
  out.set(raw, 12 + ij.length);
  return out;
}

export function packEmbedModule(lut) {
  const bytes = lutBytes(lut);
  const safe = safetensorU8("embed.lut", bytes, [BUCKETS, DIM / 4]);
  return packRawModule(safe);
}

export function unpackEmbedModule(bnz) {
  const safe = decompressSafetensors(bnz);
  const n = Number(new DataView(safe.buffer, safe.byteOffset, safe.byteLength).getBigUint64(0, true));
  const payload = safe.subarray(8 + n);
  const copy = payload.slice();
  return new Uint32Array(copy.buffer, copy.byteOffset, copy.byteLength / 4);
}

export async function embedGpu(ids, lut, shader) {
  if (!navigator.gpu) throw new Error("no gpu");
  const adapter = await navigator.gpu.requestAdapter();
  if (!adapter) throw new Error("no adapter");
  const device = await adapter.requestDevice();
  const module = device.createShaderModule({ code: shader });
  const info = await module.getCompilationInfo();
  const bad = info.messages.filter((m) => m.type === "error");
  if (bad.length) throw new Error(bad.map((m) => m.message).join("\n"));
  const tokens = new Uint32Array(ids);
  const tokenBuf = device.createBuffer({ size: Math.max(16, tokens.byteLength), usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST });
  const lutBuf = device.createBuffer({ size: lut.byteLength, usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST });
  const outBuf = device.createBuffer({ size: DIM * 4, usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_SRC });
  const readBuf = device.createBuffer({ size: DIM * 4, usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ });
  device.queue.writeBuffer(tokenBuf, 0, tokens);
  device.queue.writeBuffer(lutBuf, 0, lut);
  const bind = device.createBindGroupLayout({
    entries: [
      { binding: 0, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 1, visibility: GPUShaderStage.COMPUTE, buffer: { type: "read-only-storage" } },
      { binding: 2, visibility: GPUShaderStage.COMPUTE, buffer: { type: "storage" } },
    ],
  });
  const layout = device.createPipelineLayout({ bindGroupLayouts: [bind] });
  const pipe = device.createComputePipeline({
    layout,
    compute: { module, entryPoint: "main", constants: { N: ids.length } },
  });
  const group = device.createBindGroup({
    layout: bind,
    entries: [
      { binding: 0, resource: { buffer: tokenBuf } },
      { binding: 1, resource: { buffer: lutBuf } },
      { binding: 2, resource: { buffer: outBuf } },
    ],
  });
  const enc = device.createCommandEncoder();
  const pass = enc.beginComputePass();
  pass.setPipeline(pipe);
  pass.setBindGroup(0, group);
  pass.dispatchWorkgroups(1);
  pass.end();
  enc.copyBufferToBuffer(outBuf, 0, readBuf, 0, DIM * 4);
  device.queue.submit([enc.finish()]);
  await readBuf.mapAsync(GPUMapMode.READ);
  const raw = new Float32Array(readBuf.getMappedRange()).slice();
  readBuf.unmap();
  return normalize(raw);
}
