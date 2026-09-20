#include "wow/tty.h"

#include <cstddef>
#include <cstring>

namespace wow {
namespace {

void PutBe16(uint8_t* p, uint16_t v) {
  p[0] = static_cast<uint8_t>(v >> 8);
  p[1] = static_cast<uint8_t>(v);
}

uint16_t GetBe16(std::span<const uint8_t> p) {
  return static_cast<uint16_t>((p[0] << 8) | p[1]);
}

void PutBe32(uint8_t* p, uint32_t v) {
  p[0] = static_cast<uint8_t>(v >> 24);
  p[1] = static_cast<uint8_t>(v >> 16);
  p[2] = static_cast<uint8_t>(v >> 8);
  p[3] = static_cast<uint8_t>(v);
}

uint32_t GetBe32(std::span<const uint8_t> p) {
  return (static_cast<uint32_t>(p[0]) << 24) |
         (static_cast<uint32_t>(p[1]) << 16) |
         (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}

}  // namespace

const char* TtyTypeName(uint8_t t) {
  switch (t) {
    case kTtyHello:
      return "hello";
    case kTtyStdin:
      return "stdin";
    case kTtyStdout:
      return "stdout";
    case kTtyStderr:
      return "stderr";
    case kTtyResize:
      return "resize";
    case kTtyExit:
      return "exit";
    case kTtyPing:
      return "ping";
    case kTtyPong:
      return "pong";
    default:
      return "type";
  }
}

std::vector<uint8_t> TtyEncode(const TtyFrame& f) {
  size_t n = f.payload.size();
  if (n > 0xffff) {
    n = 0xffff;
  }
  std::vector<uint8_t> out(static_cast<size_t>(kTtyHeaderSize) + n);
  out[0] = f.type;
  out[1] = f.flags;
  PutBe16(&out[2], static_cast<uint16_t>(n));
  if (n) {
    std::memcpy(&out[4], f.payload.data(), n);
  }
  return out;
}

bool TtyDecode(std::span<const uint8_t> p, TtyFrame* out) {
  if (!out || p.size() < static_cast<size_t>(kTtyHeaderSize)) {
    return false;
  }
  int n = static_cast<int>(GetBe16(p.subspan(2, 2)));
  if (static_cast<int>(p.size()) < kTtyHeaderSize + n) {
    return false;
  }
  out->type = p[0];
  out->flags = p[1];
  out->payload.assign(p.begin() + kTtyHeaderSize,
                      p.begin() + kTtyHeaderSize + n);
  return true;
}

std::vector<uint8_t> TtyEncodeHello(TtyHello h) {
  if (h.version == 0) {
    h.version = kTtyVersion;
  }
  std::vector<uint8_t> p(10);
  std::memcpy(p.data(), kTtyMagic, 4);
  p[4] = h.version;
  p[5] = h.role;
  PutBe16(&p[6], static_cast<uint16_t>(h.cols));
  PutBe16(&p[8], static_cast<uint16_t>(h.rows));
  return p;
}

bool TtyDecodeHello(std::span<const uint8_t> p, TtyHello* out) {
  if (!out || p.size() < 10) {
    return false;
  }
  if (std::memcmp(p.data(), kTtyMagic, 4) != 0) {
    return false;
  }
  if (p[4] != kTtyVersion) {
    return false;
  }
  out->version = p[4];
  out->role = p[5];
  out->cols = static_cast<int>(GetBe16(p.subspan(6, 2)));
  out->rows = static_cast<int>(GetBe16(p.subspan(8, 2)));
  return true;
}

std::vector<uint8_t> TtyEncodeResize(int cols, int rows) {
  std::vector<uint8_t> p(4);
  PutBe16(&p[0], static_cast<uint16_t>(cols));
  PutBe16(&p[2], static_cast<uint16_t>(rows));
  return p;
}

bool TtyDecodeResize(std::span<const uint8_t> p, int* cols, int* rows) {
  if (!cols || !rows || p.size() < 4) {
    return false;
  }
  *cols = static_cast<int>(GetBe16(p.subspan(0, 2)));
  *rows = static_cast<int>(GetBe16(p.subspan(2, 2)));
  return true;
}

std::vector<uint8_t> TtyEncodeExit(int code) {
  std::vector<uint8_t> p(4);
  PutBe32(&p[0], static_cast<uint32_t>(static_cast<int32_t>(code)));
  return p;
}

bool TtyDecodeExit(std::span<const uint8_t> p, int* code) {
  if (!code || p.size() < 4) {
    return false;
  }
  *code = static_cast<int>(static_cast<int32_t>(GetBe32(p)));
  return true;
}

std::vector<TtyFrame> TtyDecoder::Push(std::span<const uint8_t> p) {
  if (p.empty()) {
    return {};
  }
  buf_.insert(buf_.end(), p.begin(), p.end());
  std::vector<TtyFrame> out;
  for (;;) {
    if (buf_.size() < static_cast<size_t>(kTtyHeaderSize)) {
      break;
    }
    int n = static_cast<int>(
        GetBe16(std::span<const uint8_t>(buf_).subspan(2, 2)));
    if (n > 0xffff) {
      buf_.clear();
      break;
    }
    size_t need = static_cast<size_t>(kTtyHeaderSize + n);
    if (buf_.size() < need) {
      break;
    }
    TtyFrame f;
    if (TtyDecode(std::span<const uint8_t>(buf_.data(), need), &f)) {
      out.push_back(std::move(f));
    }
    buf_.erase(buf_.begin(), buf_.begin() + static_cast<std::ptrdiff_t>(need));
  }
  return out;
}

void TtySession::Push(std::span<const uint8_t> p) {
  for (const auto& f : dec_.Push(p)) {
    dispatch(f);
  }
}

WowResult TtySession::Send(const TtyFrame& f) {
  if (!send_) {
    return WOW_RESULT_OK;
  }
  auto bytes = TtyEncode(f);
  return send_(bytes);
}

WowResult TtySession::WriteHello(TtyHello h) {
  hello_ = h;
  if (hello_.version == 0) {
    hello_.version = kTtyVersion;
  }
  TtyFrame f;
  f.type = kTtyHello;
  f.payload = TtyEncodeHello(hello_);
  return Send(f);
}

WowResult TtySession::WriteStdin(std::span<const uint8_t> p) {
  return writeChunks(kTtyStdin, p);
}

WowResult TtySession::WriteStdout(std::span<const uint8_t> p) {
  return writeChunks(kTtyStdout, p);
}

WowResult TtySession::WriteStderr(std::span<const uint8_t> p) {
  return writeChunks(kTtyStderr, p);
}

WowResult TtySession::WriteResize(int cols, int rows) {
  TtyFrame f;
  f.type = kTtyResize;
  f.payload = TtyEncodeResize(cols, rows);
  return Send(f);
}

WowResult TtySession::WriteExit(int code) {
  TtyFrame f;
  f.type = kTtyExit;
  f.payload = TtyEncodeExit(code);
  return Send(f);
}

WowResult TtySession::WritePing() {
  TtyFrame f;
  f.type = kTtyPing;
  return Send(f);
}

WowResult TtySession::writeChunks(uint8_t typ, std::span<const uint8_t> p) {
  if (p.empty()) {
    TtyFrame f;
    f.type = typ;
    return Send(f);
  }
  while (!p.empty()) {
    size_t n = p.size();
    if (n > static_cast<size_t>(kTtyMaxPayload)) {
      n = static_cast<size_t>(kTtyMaxPayload);
    }
    TtyFrame f;
    f.type = typ;
    f.payload.assign(p.begin(), p.begin() + static_cast<std::ptrdiff_t>(n));
    WowResult r = Send(f);
    if (r != WOW_RESULT_OK) {
      return r;
    }
    p = p.subspan(n);
  }
  return WOW_RESULT_OK;
}

void TtySession::dispatch(const TtyFrame& f) {
  switch (f.type) {
    case kTtyHello: {
      TtyHello h;
      if (!TtyDecodeHello(f.payload, &h)) {
        return;
      }
      hello_ = h;
      if (OnHello) {
        OnHello(h);
      }
      break;
    }
    case kTtyStdin:
      if (OnStdin) {
        OnStdin(f.payload);
      }
      break;
    case kTtyStdout:
      if (OnStdout) {
        OnStdout(f.payload);
      }
      break;
    case kTtyStderr:
      if (OnStderr) {
        OnStderr(f.payload);
      }
      break;
    case kTtyResize: {
      int c = 0;
      int r = 0;
      if (!TtyDecodeResize(f.payload, &c, &r)) {
        return;
      }
      if (OnResize) {
        OnResize(c, r);
      }
      break;
    }
    case kTtyExit: {
      int code = 0;
      if (!TtyDecodeExit(f.payload, &code)) {
        return;
      }
      if (OnExit) {
        OnExit(code);
      }
      break;
    }
    case kTtyPing:
      Send(TtyFrame{kTtyPong, 0, {}});
      pong_ = true;
      if (OnPing) {
        OnPing();
      }
      break;
    case kTtyPong:
      pong_ = true;
      if (OnPong) {
        OnPong();
      }
      break;
    default:
      break;
  }
}

}  // namespace wow
