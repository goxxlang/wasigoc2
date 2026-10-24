#include "wns/tty.h"

#include <algorithm>

namespace wns {
namespace {

void Put16(std::vector<uint8_t>* o, uint16_t v) {
  o->push_back(uint8_t(v >> 8));
  o->push_back(uint8_t(v));
}

void Put32(std::vector<uint8_t>* o, uint32_t v) {
  o->push_back(uint8_t(v >> 24));
  o->push_back(uint8_t(v >> 16));
  o->push_back(uint8_t(v >> 8));
  o->push_back(uint8_t(v));
}

uint16_t Get16(std::span<const uint8_t> p, size_t at) {
  return uint16_t((uint16_t(p[at]) << 8) | p[at + 1]);
}

uint32_t Get32(std::span<const uint8_t> p, size_t at) {
  return (uint32_t(p[at]) << 24) | (uint32_t(p[at + 1]) << 16) |
         (uint32_t(p[at + 2]) << 8) | uint32_t(p[at + 3]);
}

void Append(std::vector<uint8_t>* dst, std::span<const uint8_t> src) {
  dst->insert(dst->end(), src.begin(), src.end());
}

}  // namespace

std::vector<uint8_t> EncodeTtyFrame(const TtyFrame& f) {
  size_t n = f.payload.size();
  if (n > 0xffff) n = 0xffff;
  std::vector<uint8_t> out(sctp::kTtyHeaderSize + n);
  out[0] = f.type;
  out[1] = f.flags;
  out[2] = uint8_t(n >> 8);
  out[3] = uint8_t(n);
  if (n) std::copy(f.payload.begin(), f.payload.begin() + n, out.begin() + sctp::kTtyHeaderSize);
  return out;
}

bool DecodeTtyFrame(std::span<const uint8_t> wire, TtyFrame* out) {
  if (out == nullptr || wire.size() < sctp::kTtyHeaderSize) return false;
  uint16_t n = Get16(wire, 2);
  if (wire.size() < sctp::kTtyHeaderSize + n) return false;
  out->type = wire[0];
  out->flags = wire[1];
  out->payload.assign(wire.begin() + sctp::kTtyHeaderSize,
                      wire.begin() + sctp::kTtyHeaderSize + n);
  return true;
}

std::vector<uint8_t> EncodeTtyHello(const TtyHello& h) {
  TtyHello x = h;
  if (x.version == 0) x.version = sctp::kTtyVersion;
  std::vector<uint8_t> p;
  p.reserve(10);
  p.push_back('W');
  p.push_back('T');
  p.push_back('T');
  p.push_back('Y');
  p.push_back(x.version);
  p.push_back(x.role);
  Put16(&p, x.cols);
  Put16(&p, x.rows);
  return p;
}

bool DecodeTtyHello(std::span<const uint8_t> wire, TtyHello* out) {
  if (out == nullptr || wire.size() < 10) return false;
  if (wire[0] != 'W' || wire[1] != 'T' || wire[2] != 'T' || wire[3] != 'Y') return false;
  if (wire[4] != sctp::kTtyVersion) return false;
  out->version = wire[4];
  out->role = wire[5];
  out->cols = Get16(wire, 6);
  out->rows = Get16(wire, 8);
  return true;
}

std::vector<uint8_t> EncodeTtyResize(uint16_t cols, uint16_t rows) {
  std::vector<uint8_t> p;
  Put16(&p, cols);
  Put16(&p, rows);
  return p;
}

bool DecodeTtyResize(std::span<const uint8_t> wire, uint16_t* cols, uint16_t* rows) {
  if (cols == nullptr || rows == nullptr || wire.size() < 4) return false;
  *cols = Get16(wire, 0);
  *rows = Get16(wire, 2);
  return true;
}

std::vector<uint8_t> EncodeTtyExit(int32_t code) {
  std::vector<uint8_t> p;
  Put32(&p, uint32_t(code));
  return p;
}

bool DecodeTtyExit(std::span<const uint8_t> wire, int32_t* code) {
  if (code == nullptr || wire.size() < 4) return false;
  *code = int32_t(Get32(wire, 0));
  return true;
}

std::vector<TtyFrame> TtyDecoder::Push(std::span<const uint8_t> p) {
  if (!p.empty()) buf_.insert(buf_.end(), p.begin(), p.end());
  std::vector<TtyFrame> out;
  for (;;) {
    if (buf_.size() < sctp::kTtyHeaderSize) break;
    uint16_t n = Get16(buf_, 2);
    size_t need = sctp::kTtyHeaderSize + n;
    if (buf_.size() < need) break;
    TtyFrame f;
    if (!DecodeTtyFrame(std::span<const uint8_t>(buf_.data(), need), &f)) {
      buf_.erase(buf_.begin(), buf_.begin() + static_cast<std::ptrdiff_t>(need));
      continue;
    }
    out.push_back(std::move(f));
    buf_.erase(buf_.begin(), buf_.begin() + static_cast<std::ptrdiff_t>(need));
  }
  return out;
}

uint16_t TtyMux::Alloc() {
  const uint16_t base = server_ ? sctp::kStreamTtyServer : sctp::kStreamTtyMin;
  const uint16_t last = server_ ? sctp::kStreamTtyMax : uint16_t(sctp::kStreamTtyServer - 1);
  if (next_ < base || next_ > last) next_ = base;
  for (uint32_t tries = 0; tries < 4096; ++tries) {
    uint16_t cand = next_;
    if (next_ == last) next_ = base;
    else ++next_;
    if (!Find(cand)) return cand;
  }
  return 0;
}

TtySession* TtyMux::Adopt(uint16_t stream) {
  if (auto* s = Find(stream)) return s;
  TtySession s;
  s.stream = stream;
  sessions_.push_back(std::move(s));
  return &sessions_.back();
}

TtySession* TtyMux::Find(uint16_t stream) {
  for (auto& s : sessions_) {
    if (s.stream == stream) return &s;
  }
  return nullptr;
}

const TtySession* TtyMux::Find(uint16_t stream) const {
  for (const auto& s : sessions_) {
    if (s.stream == stream) return &s;
  }
  return nullptr;
}

bool TtyMux::Emit(uint16_t stream, const TtyFrame& f) {
  if (!send_ || !sctp::IsTtyStream(stream)) return false;
  auto wire = EncodeTtyFrame(f);
  send_(stream, sctp::kPpidTty, wire, false);
  return true;
}

bool TtyMux::WriteChunks(uint16_t stream, uint8_t type, std::span<const uint8_t> data) {
  if (!Find(stream)) return false;
  if (data.empty()) {
    TtyFrame f;
    f.type = type;
    return Emit(stream, f);
  }
  size_t off = 0;
  while (off < data.size()) {
    size_t n = data.size() - off;
    if (n > sctp::kTtyMaxPayload) n = sctp::kTtyMaxPayload;
    TtyFrame f;
    f.type = type;
    f.payload.assign(data.begin() + static_cast<std::ptrdiff_t>(off),
                     data.begin() + static_cast<std::ptrdiff_t>(off + n));
    if (!Emit(stream, f)) return false;
    off += n;
  }
  return true;
}

uint16_t TtyMux::Open(uint16_t cols, uint16_t rows) {
  uint16_t sid = Alloc();
  if (sid == 0) return 0;
  auto* s = Adopt(sid);
  s->role = sctp::kTtyRoleListener;
  s->cols = cols;
  s->rows = rows;
  s->hello_sent = true;
  TtyHello h;
  h.version = sctp::kTtyVersion;
  h.role = sctp::kTtyRoleListener;
  h.cols = cols;
  h.rows = rows;
  TtyFrame f;
  f.type = sctp::kTtyHello;
  f.payload = EncodeTtyHello(h);
  if (!Emit(sid, f)) return 0;
  return sid;
}

bool TtyMux::Stdin(uint16_t stream, std::span<const uint8_t> data) {
  return WriteChunks(stream, sctp::kTtyStdin, data);
}
bool TtyMux::Stdout(uint16_t stream, std::span<const uint8_t> data) {
  return WriteChunks(stream, sctp::kTtyStdout, data);
}
bool TtyMux::Stderr(uint16_t stream, std::span<const uint8_t> data) {
  return WriteChunks(stream, sctp::kTtyStderr, data);
}

bool TtyMux::Resize(uint16_t stream, uint16_t cols, uint16_t rows) {
  if (!Find(stream)) return false;
  TtyFrame f;
  f.type = sctp::kTtyResize;
  f.payload = EncodeTtyResize(cols, rows);
  return Emit(stream, f);
}

bool TtyMux::Exit(uint16_t stream, int32_t code) {
  if (!Find(stream)) return false;
  TtyFrame f;
  f.type = sctp::kTtyExit;
  f.payload = EncodeTtyExit(code);
  return Emit(stream, f);
}

bool TtyMux::Ping(uint16_t stream) {
  if (!Find(stream)) return false;
  TtyFrame f;
  f.type = sctp::kTtyPing;
  return Emit(stream, f);
}

void TtyMux::Dispatch(TtySession* s, const TtyFrame& f) {
  switch (f.type) {
    case sctp::kTtyHello: {
      TtyHello h;
      if (!DecodeTtyHello(f.payload, &h)) return;
      s->have_hello = true;
      s->role = h.role;
      s->cols = h.cols;
      s->rows = h.rows;
      if (!s->hello_sent) {
        s->hello_sent = true;
        TtyHello reply;
        reply.version = sctp::kTtyVersion;
        reply.role = sctp::kTtyRoleReceiver;
        reply.cols = h.cols;
        reply.rows = h.rows;
        TtyFrame out;
        out.type = sctp::kTtyHello;
        out.payload = EncodeTtyHello(reply);
        Emit(s->stream, out);
      }
      break;
    }
    case sctp::kTtyStdin:
      Append(&s->stdin_bytes, f.payload);
      break;
    case sctp::kTtyStdout:
      Append(&s->stdout_bytes, f.payload);
      break;
    case sctp::kTtyStderr:
      Append(&s->stderr_bytes, f.payload);
      break;
    case sctp::kTtyResize: {
      uint16_t c = 0, r = 0;
      if (!DecodeTtyResize(f.payload, &c, &r)) return;
      s->cols = c;
      s->rows = r;
      break;
    }
    case sctp::kTtyExit:
      if (!DecodeTtyExit(f.payload, &s->exit_code)) return;
      s->exited = true;
      break;
    case sctp::kTtyPing: {
      ++s->pings;
      TtyFrame pong;
      pong.type = sctp::kTtyPong;
      Emit(s->stream, pong);
      break;
    }
    case sctp::kTtyPong:
      ++s->pongs;
      break;
    default:
      break;
  }
}

bool TtyMux::OnWire(std::span<const uint8_t> user, uint16_t sid) {
  if (!sctp::IsTtyStream(sid)) return false;
  auto* s = Adopt(sid);
  for (const auto& f : s->dec.Push(user)) Dispatch(s, f);
  return true;
}

}  // namespace wns
