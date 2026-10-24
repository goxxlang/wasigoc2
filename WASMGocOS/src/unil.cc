#include "gocos/unil.hpp"

#include <cstring>

namespace wasmgocos {
namespace {

// The payload a bundle may inflate to. A container is one cage object.
constexpr size_t kMaxPayload = size_t{256} << 20;
constexpr size_t kHeader = 32;

// LSB-first bit reader over the deflate stream. Holds at most 7 bits
// between reads, so dropping them is byte alignment.
struct Bits {
  const uint8_t* p;
  size_t n;
  size_t pos = 0;
  uint32_t buf = 0;
  int cnt = 0;
  bool bad = false;

  uint32_t Get(int k) {
    while (cnt < k) {
      if (pos >= n) {
        bad = true;
        return 0;
      }
      buf |= static_cast<uint32_t>(p[pos++]) << cnt;
      cnt += 8;
    }
    uint32_t v = buf & ((1u << k) - 1);
    buf >>= k;
    cnt -= k;
    return v;
  }
  void Align() {
    buf = 0;
    cnt = 0;
  }
};

// Canonical Huffman code: how many codes of each length, and the symbols
// in code order.
struct Huff {
  uint16_t count[16];
  uint16_t sym[288];
};

bool Build(Huff* h, const uint8_t* lens, int n) {
  std::memset(h->count, 0, sizeof(h->count));
  for (int i = 0; i < n; ++i) h->count[lens[i]]++;
  h->count[0] = 0;
  int left = 1;
  for (int len = 1; len < 16; ++len) {
    left <<= 1;
    left -= h->count[len];
    if (left < 0) return false;  // over-subscribed
  }
  uint16_t offs[16];
  offs[1] = 0;
  for (int len = 1; len < 15; ++len) offs[len + 1] = static_cast<uint16_t>(offs[len] + h->count[len]);
  for (int i = 0; i < n; ++i) {
    if (lens[i] != 0) h->sym[offs[lens[i]]++] = static_cast<uint16_t>(i);
  }
  return true;
}

int Decode(Bits* b, const Huff& h) {
  int code = 0, first = 0, index = 0;
  for (int len = 1; len < 16; ++len) {
    code |= static_cast<int>(b->Get(1));
    if (b->bad) return -1;
    int count = h.count[len];
    if (code - count < first) return h.sym[index + (code - first)];
    index += count;
    first += count;
    first <<= 1;
    code <<= 1;
  }
  return -1;
}

constexpr uint16_t kLenBase[29] = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
                                   31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
constexpr uint8_t kLenExtra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                   2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
constexpr uint16_t kDistBase[30] = {1,    2,    3,    4,    5,    7,     9,     13,    17,  25,
                                    33,   49,   65,   97,   129,  193,   257,   385,   513, 769,
                                    1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
constexpr uint8_t kDistExtra[30] = {0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
                                    6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

bool Codes(Bits* b, const Huff& lit, const Huff& dist, std::vector<uint8_t>* out) {
  for (;;) {
    int sym = Decode(b, lit);
    if (sym < 0) return false;
    if (sym < 256) {
      if (out->size() >= kMaxPayload) return false;
      out->push_back(static_cast<uint8_t>(sym));
      continue;
    }
    if (sym == 256) return true;
    sym -= 257;
    if (sym >= 29) return false;
    size_t len = kLenBase[sym] + b->Get(kLenExtra[sym]);
    int ds = Decode(b, dist);
    if (ds < 0 || ds >= 30) return false;
    size_t d = kDistBase[ds] + b->Get(kDistExtra[ds]);
    if (b->bad || d > out->size() || out->size() + len > kMaxPayload) return false;
    size_t from = out->size() - d;
    for (size_t i = 0; i < len; ++i) out->push_back((*out)[from + i]);
  }
}

bool Stored(Bits* b, std::vector<uint8_t>* out) {
  b->Align();
  if (b->n - b->pos < 4) return false;
  const uint8_t* p = b->p + b->pos;
  uint16_t len = static_cast<uint16_t>(p[0] | (p[1] << 8));
  uint16_t nlen = static_cast<uint16_t>(p[2] | (p[3] << 8));
  if (static_cast<uint16_t>(~nlen) != len) return false;
  b->pos += 4;
  if (b->n - b->pos < len || out->size() + len > kMaxPayload) return false;
  out->insert(out->end(), b->p + b->pos, b->p + b->pos + len);
  b->pos += len;
  return true;
}

bool Fixed(Bits* b, std::vector<uint8_t>* out) {
  static const auto tables = [] {
    struct T {
      Huff lit, dist;
    } t;
    uint8_t lens[288];
    int i = 0;
    for (; i < 144; ++i) lens[i] = 8;
    for (; i < 256; ++i) lens[i] = 9;
    for (; i < 280; ++i) lens[i] = 7;
    for (; i < 288; ++i) lens[i] = 8;
    Build(&t.lit, lens, 288);
    for (i = 0; i < 30; ++i) lens[i] = 5;
    Build(&t.dist, lens, 30);
    return t;
  }();
  return Codes(b, tables.lit, tables.dist, out);
}

bool Dynamic(Bits* b, std::vector<uint8_t>* out) {
  static constexpr uint8_t kOrder[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
  int nlit = static_cast<int>(b->Get(5)) + 257;
  int ndist = static_cast<int>(b->Get(5)) + 1;
  int ncode = static_cast<int>(b->Get(4)) + 4;
  if (b->bad || nlit > 286 || ndist > 30) return false;
  uint8_t lens[320] = {};
  for (int i = 0; i < ncode; ++i) lens[kOrder[i]] = static_cast<uint8_t>(b->Get(3));
  if (b->bad) return false;
  Huff code;
  if (!Build(&code, lens, 19)) return false;
  std::memset(lens, 0, sizeof(lens));
  int at = 0;
  while (at < nlit + ndist) {
    int sym = Decode(b, code);
    if (sym < 0) return false;
    if (sym < 16) {
      lens[at++] = static_cast<uint8_t>(sym);
      continue;
    }
    uint8_t rep = 0;
    int times = 0;
    if (sym == 16) {
      if (at == 0) return false;
      rep = lens[at - 1];
      times = 3 + static_cast<int>(b->Get(2));
    } else if (sym == 17) {
      times = 3 + static_cast<int>(b->Get(3));
    } else {
      times = 11 + static_cast<int>(b->Get(7));
    }
    if (b->bad || at + times > nlit + ndist) return false;
    while (times-- > 0) lens[at++] = rep;
  }
  if (lens[256] == 0) return false;  // no end-of-block code
  Huff lit, dist;
  if (!Build(&lit, lens, nlit) || !Build(&dist, lens + nlit, ndist)) return false;
  return Codes(b, lit, dist, out);
}

// Raw deflate. `used` is how many input bytes the stream took.
bool Inflate(const uint8_t* data, size_t n, std::vector<uint8_t>* out, size_t* used) {
  Bits b{data, n};
  int last = 0;
  do {
    last = static_cast<int>(b.Get(1));
    int type = static_cast<int>(b.Get(2));
    if (b.bad) return false;
    bool ok = false;
    if (type == 0) ok = Stored(&b, out);
    else if (type == 1) ok = Fixed(&b, out);
    else if (type == 2) ok = Dynamic(&b, out);
    if (!ok || b.bad) return false;
  } while (!last);
  *used = b.pos;
  return true;
}

uint32_t U32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

uint64_t U64(const uint8_t* p) {
  return static_cast<uint64_t>(U32(p)) | (static_cast<uint64_t>(U32(p + 4)) << 32);
}

void PutU32(uint8_t* p, uint32_t v) {
  for (int i = 0; i < 4; ++i) p[i] = static_cast<uint8_t>(v >> (8 * i));
}

void PutU64(uint8_t* p, uint64_t v) {
  for (int i = 0; i < 8; ++i) p[i] = static_cast<uint8_t>(v >> (8 * i));
}

bool Fail(std::string* why, const char* what) {
  if (why != nullptr) *why = what;
  return false;
}

// LSB-first bit writer, the mirror of Bits.
struct Sink {
  std::vector<uint8_t>* out;
  uint32_t buf = 0;
  int cnt = 0;

  void Put(uint32_t v, int k) {  // k <= 16
    buf |= v << cnt;
    cnt += k;
    while (cnt >= 8) {
      out->push_back(static_cast<uint8_t>(buf));
      buf >>= 8;
      cnt -= 8;
    }
  }
  // Huffman codes are packed most significant bit first.
  void Code(uint32_t code, int len) {
    uint32_t rev = 0;
    for (int i = 0; i < len; ++i) rev |= ((code >> i) & 1u) << (len - 1 - i);
    Put(rev, len);
  }
  void Flush() {
    if (cnt > 0) out->push_back(static_cast<uint8_t>(buf));
    buf = 0;
    cnt = 0;
  }
};

// The fixed literal/length code of RFC 1951 3.2.6.
void Lit(Sink* s, int sym) {
  if (sym < 144) s->Code(0x30 + sym, 8);
  else if (sym < 256) s->Code(0x190 + sym - 144, 9);
  else if (sym < 280) s->Code(sym - 256, 7);
  else s->Code(0xc0 + sym - 280, 8);
}

constexpr size_t kWindow = 32768;
constexpr int kHashBits = 15;
constexpr int kChain = 128;
constexpr size_t kMaxMatch = 258;

uint32_t Hash3(const uint8_t* p) {
  uint32_t v = static_cast<uint32_t>(p[0]) << 16 | static_cast<uint32_t>(p[1]) << 8 | p[2];
  return (v * 2654435761u) >> (32 - kHashBits);
}

// One final fixed-Huffman block. Greedy matching on a hash chain over the
// last 32K.
void Deflate(const uint8_t* data, size_t n, std::vector<uint8_t>* out) {
  Sink s{out};
  s.Put(1, 1);  // BFINAL
  s.Put(1, 2);  // BTYPE 01
  std::vector<int32_t> head(size_t{1} << kHashBits, -1);
  std::vector<int32_t> prev(kWindow, -1);
  auto insert = [&](size_t at) {
    if (at + 3 > n) return;
    uint32_t h = Hash3(data + at);
    prev[at & (kWindow - 1)] = head[h];
    head[h] = static_cast<int32_t>(at);
  };
  size_t i = 0;
  while (i < n) {
    size_t best = 0;
    size_t dist = 0;
    if (i + 3 <= n) {
      size_t limit = n - i < kMaxMatch ? n - i : kMaxMatch;
      int32_t cand = head[Hash3(data + i)];
      for (int chain = kChain; cand >= 0 && chain > 0; --chain) {
        size_t c = static_cast<size_t>(cand);
        if (i - c > kWindow) break;
        size_t len = 0;
        while (len < limit && data[c + len] == data[i + len]) ++len;
        if (len > best) {
          best = len;
          dist = i - c;
          if (len == limit) break;
        }
        int32_t next = prev[c & (kWindow - 1)];
        if (next >= cand) break;  // the slot belongs to a newer position
        cand = next;
      }
    }
    if (best < 3) {
      Lit(&s, data[i]);
      insert(i);
      ++i;
      continue;
    }
    int li = 28;
    while (kLenBase[li] > best) --li;
    Lit(&s, 257 + li);
    s.Put(static_cast<uint32_t>(best - kLenBase[li]), kLenExtra[li]);
    int di = 29;
    while (kDistBase[di] > dist) --di;
    s.Code(static_cast<uint32_t>(di), 5);
    s.Put(static_cast<uint32_t>(dist - kDistBase[di]), kDistExtra[di]);
    for (size_t k = 0; k < best; ++k) insert(i + k);
    i += best;
  }
  Lit(&s, 256);
  s.Flush();
}

}  // namespace

uint32_t unil_crc32(const uint8_t* data, size_t n) {
  static const auto table = [] {
    struct T {
      uint32_t v[256];
    } t;
    for (uint32_t i = 0; i < 256; ++i) {
      uint32_t c = i;
      for (int k = 0; k < 8; ++k) c = (c >> 1) ^ (0xEDB88320u & (~(c & 1) + 1));
      t.v[i] = c;
    }
    return t;
  }();
  uint32_t crc = 0xffffffffu;
  for (size_t i = 0; i < n; ++i) crc = table.v[(crc ^ data[i]) & 0xff] ^ (crc >> 8);
  return crc ^ 0xffffffffu;
}

bool unil_gunzip(const uint8_t* data, size_t n, std::vector<uint8_t>* out, std::string* why) {
  if (n < 18 || data[0] != 0x1f || data[1] != 0x8b) return Fail(why, "gzip magic");
  if (data[2] != 8) return Fail(why, "gzip method");
  uint8_t flg = data[3];
  if (flg & 0xe0) return Fail(why, "gzip flags");
  size_t pos = 10;
  if (flg & 0x04) {  // FEXTRA
    if (n - pos < 2) return Fail(why, "gzip header");
    size_t xlen = data[pos] | (data[pos + 1] << 8);
    pos += 2;
    if (n - pos < xlen) return Fail(why, "gzip header");
    pos += xlen;
  }
  for (uint8_t bit : {uint8_t{0x08}, uint8_t{0x10}}) {  // FNAME, FCOMMENT
    if (!(flg & bit)) continue;
    while (pos < n && data[pos] != 0) ++pos;
    if (pos >= n) return Fail(why, "gzip header");
    ++pos;
  }
  if (flg & 0x02) pos += 2;  // FHCRC
  if (pos + 8 > n) return Fail(why, "gzip header");
  out->clear();
  size_t used = 0;
  if (!Inflate(data + pos, n - pos - 8, out, &used)) return Fail(why, "deflate");
  const uint8_t* trailer = data + pos + used;
  if (static_cast<size_t>(data + n - trailer) < 8) return Fail(why, "gzip trailer");
  if (U32(trailer) != unil_crc32(out->data(), out->size())) return Fail(why, "gzip crc");
  if (U32(trailer + 4) != static_cast<uint32_t>(out->size())) return Fail(why, "gzip size");
  return true;
}

bool unil_plain(const uint8_t* data, size_t n, std::vector<uint8_t>* plain, std::string* why) {
  if (n < kHeader || std::memcmp(data, "UNIL", 4) != 0) return Fail(why, "magic");
  if ((data[4] | (data[5] << 8)) != 1) return Fail(why, "version");
  if (U32(data + 28) != unil_crc32(data, 28)) return Fail(why, "header crc");
  uint16_t flags = static_cast<uint16_t>(data[6] | (data[7] << 8));
  uint32_t count = U32(data + 8);
  uint64_t index_at = U64(data + 12);
  uint64_t data_at = U64(data + 20);
  if (index_at < kHeader || index_at > data_at || data_at > n) return Fail(why, "offsets");

  std::vector<uint8_t> payload;
  if (flags & 1) {
    if (!unil_gunzip(data + data_at, n - data_at, &payload, why)) return false;
  } else {
    payload.assign(data + data_at, data + n);
  }
  if (payload.size() > kMaxPayload) return Fail(why, "payload size");

  // Walk the index against the payload. Every region has to land inside it
  // and hash to its CRC.
  size_t pos = index_at;
  for (uint32_t i = 0; i < count; ++i) {
    for (int str = 0; str < 2; ++str) {  // name, mime
      if (data_at - pos < 2) return Fail(why, "index");
      size_t len = data[pos] | (data[pos + 1] << 8);
      pos += 2;
      if (data_at - pos < len) return Fail(why, "index");
      if (str == 0 && len == 0) return Fail(why, "region name");
      pos += len;
    }
    if (data_at - pos < 22) return Fail(why, "index");
    uint64_t off = U64(data + pos + 2);
    uint64_t size = U64(data + pos + 10);
    uint32_t crc = U32(data + pos + 18);
    pos += 22;
    if (off > payload.size() || size > payload.size() - off) return Fail(why, "region bounds");
    if (unil_crc32(payload.data() + off, size) != crc) return Fail(why, "region crc");
  }

  // Header, the index bytes as they were, the inflated payload.
  size_t index_n = pos - index_at;
  plain->assign(kHeader, 0);
  std::memcpy(plain->data(), "UNIL", 4);
  (*plain)[4] = 1;
  PutU32(plain->data() + 8, count);
  PutU64(plain->data() + 12, kHeader);
  PutU64(plain->data() + 20, kHeader + index_n);
  PutU32(plain->data() + 28, unil_crc32(plain->data(), 28));
  plain->insert(plain->end(), data + index_at, data + pos);
  plain->insert(plain->end(), payload.begin(), payload.end());
  return true;
}

std::vector<uint8_t> unil_gzip(const uint8_t* data, size_t n) {
  // No name, no mtime, OS unknown.
  std::vector<uint8_t> out = {0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff};
  Deflate(data, n, &out);
  uint8_t trailer[8];
  PutU32(trailer, unil_crc32(data, n));
  PutU32(trailer + 4, static_cast<uint32_t>(n));
  out.insert(out.end(), trailer, trailer + 8);
  return out;
}

std::vector<uint8_t> unil_pack(const std::vector<UnilPart>& parts) {
  std::vector<uint8_t> index;
  std::vector<uint8_t> payload;
  for (const UnilPart& part : parts) {
    if (part.name.empty()) return {};
    for (const std::string* str : {&part.name, &part.mime}) {
      if (str->size() > 0xffff) return {};
      index.push_back(static_cast<uint8_t>(str->size()));
      index.push_back(static_cast<uint8_t>(str->size() >> 8));
      index.insert(index.end(), str->begin(), str->end());
    }
    uint8_t entry[22];
    entry[0] = part.kind;
    entry[1] = part.flags;
    PutU64(entry + 2, payload.size());
    PutU64(entry + 10, part.size);
    PutU32(entry + 18, unil_crc32(part.data, part.size));
    index.insert(index.end(), entry, entry + sizeof(entry));
    if (part.size > 0) payload.insert(payload.end(), part.data, part.data + part.size);
  }
  if (payload.size() > kMaxPayload) return {};
  std::vector<uint8_t> out(kHeader, 0);
  std::memcpy(out.data(), "UNIL", 4);
  out[4] = 1;
  out[6] = 1;  // gzip payload
  PutU32(out.data() + 8, static_cast<uint32_t>(parts.size()));
  PutU64(out.data() + 12, kHeader);
  PutU64(out.data() + 20, kHeader + index.size());
  PutU32(out.data() + 28, unil_crc32(out.data(), 28));
  out.insert(out.end(), index.begin(), index.end());
  std::vector<uint8_t> gz = unil_gzip(payload.data(), payload.size());
  out.insert(out.end(), gz.begin(), gz.end());
  return out;
}

}  // namespace wasmgocos
