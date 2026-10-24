// bnz4dec.js: BNZ4 segment decoder that leaves the decoded tensor on the GPU.
// From bnz-loader/js/bnz4dec.js (same kernels, same
// container). decode() returns the device buffer holding the raw bytes and, with readRaw, the bytes
// read back. Champ uses the read-back bytes: they are sha256-checked and land in the weight heap
// (bnz4load.js), which the guest reads as model.safetensors.
import { TERN_WGSL, BF16_WGSL } from './bnz4shaders.js';

const S = 0x80, CDST = 0x8, CSRC = 0x4;

function upk(b) {
  const dv = new DataView(b.buffer, b.byteOffset, b.byteLength);
  const out = [];
  for (let i = 0; i < b.length;) {
    if (i + 8 > b.length) throw new Error('bad pack');
    const n = Number(dv.getBigUint64(i, true)); i += 8;
    if (i + n > b.length) throw new Error('bad pack');
    out.push(b.subarray(i, i + n)); i += n;
  }
  return out;
}
const u32 = (b) => new Uint32Array(b.buffer.slice(b.byteOffset, b.byteOffset + (b.byteLength & ~3)));
const le32 = (b, o) => new DataView(b.buffer, b.byteOffset, b.byteLength).getUint32(o, true);

function ternW(rc, N) {
  const s = [0, 0, 0];
  for (let n = 0; n < N; n++) for (let k = 0; k < 3; k++) s[k] += rc[3 * n + k];
  const g2 = s.map((v) => 2n * BigInt(v) + 1n);
  const mn = g2.reduce((a, b) => (b < a ? b : a));
  return g2.map((g) => Number((0xFFFFFFFFn * mn) / g));
}

export async function sha256hex(b) {
  const h = new Uint8Array(await crypto.subtle.digest('SHA-256', b));
  return Array.from(h, (x) => x.toString(16).padStart(2, '0')).join('');
}

export class BNZ4Dec {
  constructor(device, zstd) {
    this.dev = device; this.zstd = zstd;
    this.tern = this.#mod(TERN_WGSL, 7, ['t_build', 't_decode']);
    this.bf = this.#mod(BF16_WGSL, 7, ['b_build', 'b_decode']);
  }

  #mod(code, nstore, entries) {
    const module = this.dev.createShaderModule({ code });
    const ents = [{ binding: 0, visibility: 4, buffer: { type: 'uniform' } }];
    for (let i = 1; i <= nstore; i++) ents.push({ binding: i, visibility: 4, buffer: { type: 'storage' } });
    const bgl = this.dev.createBindGroupLayout({ entries: ents });
    const layout = this.dev.createPipelineLayout({ bindGroupLayouts: [bgl] });
    const pipes = {};
    for (const e of entries) pipes[e] = this.dev.createComputePipeline({ layout, compute: { module, entryPoint: e } });
    return { bgl, pipes };
  }

  #buf(data, size = 0) {
    const n = Math.max(4, (Math.max(size, data ? data.byteLength : 0) + 3) & ~3);
    const b = this.dev.createBuffer({ size: n, usage: S | CDST | CSRC });
    if (data && data.byteLength) {
      let d = data;
      if (d.byteLength & 3) { const p = new Uint8Array((d.byteLength + 3) & ~3); p.set(d); d = p; }
      this.dev.queue.writeBuffer(b, 0, d);
    }
    return b;
  }

  #uniform(v) {
    const a = new Uint32Array((v.length + 3) & ~3); a.set(v);
    const b = this.dev.createBuffer({ size: a.byteLength, usage: 0x40 | CDST });
    this.dev.queue.writeBuffer(b, 0, a);
    return b;
  }

  #run(m, buffers, steps) {
    const bg = this.dev.createBindGroup({ layout: m.bgl, entries: buffers.map((buffer, binding) => ({ binding, resource: { buffer } })) });
    const enc = this.dev.createCommandEncoder();
    for (const [entry, threads] of steps) {
      const p = enc.beginComputePass();
      p.setPipeline(m.pipes[entry]); p.setBindGroup(0, bg);
      const wgs = Math.max(1, Math.ceil(threads / 64)); const x = Math.min(wgs, 65535);
      p.dispatchWorkgroups(x, Math.ceil(wgs / x)); p.end();
    }
    this.dev.queue.submit([enc.finish()]);
  }

  async read(src, off, size) {
    const n = Math.max(4, (size + 3) & ~3);
    const st = this.dev.createBuffer({ size: n, usage: 0x1 | CDST });
    const enc = this.dev.createCommandEncoder();
    enc.copyBufferToBuffer(src, off, st, 0, n);
    this.dev.queue.submit([enc.finish()]);
    await st.mapAsync(1);
    const out = new Uint8Array(st.getMappedRange()).slice(0, size);
    st.unmap(); st.destroy();
    return out;
  }

  async #tern(c, R, C, readRaw) {
    const p = upk(c);
    if (p.length !== 4 || p[0].length !== 12) throw new Error('tern4: bad payload');
    const mode = le32(p[0], 0), rpc = le32(p[0], 4), nch = le32(p[0], 8), N = 4 * R;
    const tv = u32(await this.zstd(p[1]));
    if (tv.length !== 3 * (N + C) || nch !== Math.ceil(R / rpc) || C % 4) throw new Error('tern4: bad tables');
    const w = ternW(tv, N);
    const bufs = [this.#uniform([R, C, N, mode, rpc, nch, w[0], w[1], w[2], 0, N + C, 0]), this.#buf(null, R * C), this.#buf(tv),
      this.#buf(null, 4 * (N + C)), this.#buf(p[3]), this.#buf(p[2]), this.#buf(null, 4 * nch), this.#buf(null, 4)];
    this.#run(this.tern, bufs, [['t_build', N + C], ['t_decode', nch]]);
    const status = u32(await this.read(bufs[6], 0, 4 * nch));
    const raw = readRaw ? await this.read(bufs[1], 0, R * C) : null;
    bufs.forEach((b, i) => { if (i !== 1) b.destroy(); });
    if (status.some((s) => s)) { bufs[1].destroy(); throw new Error('tern4: stream overrun'); }
    return { gpu: bufs[1], raw };
  }

  async #bf16(c, shape, readRaw) {
    const n = shape.reduce((a, b) => a * b, 1);
    const p = upk(c);
    if (p.length !== 5 || p[0].length !== 20) throw new Error('bf16x: bad payload');
    const emin = le32(p[0], 0), K = le32(p[0], 4), C = le32(p[0], 8), epc = le32(p[0], 12), nch = le32(p[0], 16);
    const M = C * K + K * 128;
    const cnt = u32(await this.zstd(p[1]));
    const sign = await this.zstd(p[2]);
    if (cnt.length !== M || sign.length !== Math.ceil(n / 8) || nch !== Math.ceil(n / epc)) throw new Error('bf16x: bad tables');
    const bufs = [this.#uniform([n, C, K, emin, epc, nch, 0, M]), this.#buf(null, 2 * n), this.#buf(cnt), this.#buf(null, 4 * (2 * M + C + K)),
      this.#buf(sign), this.#buf(p[4]), this.#buf(p[3]), this.#buf(null, 4 * (2 + nch))];
    this.#run(this.bf, bufs, [['b_build', C + K], ['b_decode', nch]]);
    const status = u32(await this.read(bufs[7], 8, 4 * nch));
    const raw = readRaw ? await this.read(bufs[1], 0, 2 * n) : null;
    bufs.forEach((b, i) => { if (i !== 1) b.destroy(); });
    if (status.some((s) => s)) { bufs[1].destroy(); throw new Error('bf16x: stream overrun'); }
    return { gpu: bufs[1], raw };
  }

  // decode -> { gpu: GPUBuffer|null (methods 4/5), raw: Uint8Array|null }.
  // Methods 0/1 are CPU (raw only). With readRaw the GPU result is also read back.
  async decode(seg, payload, readRaw = true) {
    switch (seg.method) {
      case 0: return { gpu: null, raw: payload };
      case 1: return { gpu: null, raw: this.zstd(payload) };
      case 4: return this.#tern(payload, seg.shape[0], seg.shape[1], readRaw);
      case 5: return this.#bf16(payload, seg.shape, readRaw);
      default: throw new Error(`${seg.name}: method ${seg.method} not supported`);
    }
  }
}
