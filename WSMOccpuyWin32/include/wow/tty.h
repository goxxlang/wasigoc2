#ifndef WOW_TTY_H_
#define WOW_TTY_H_

#include "wow/c/types.h"

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace wow {

// WASMTTY frame proto, loaded from ~/WASMTTY/tty/proto.go. Occupancy
// Calls this hop; it does not rewrite the listener/receiver. Once sys
// is occupied, this wasigocvm is the toolkit (WASMWin32 /
// CreateProcessW / WASMTTY) — one VM, not a second gocvm.exe.
// Occupancy Calls that hop; it does not steal tty.*.

inline constexpr uint8_t kTtyHello = 0x01;
inline constexpr uint8_t kTtyStdin = 0x02;
inline constexpr uint8_t kTtyStdout = 0x03;
inline constexpr uint8_t kTtyStderr = 0x04;
inline constexpr uint8_t kTtyResize = 0x05;
inline constexpr uint8_t kTtyExit = 0x06;
inline constexpr uint8_t kTtyPing = 0x07;
inline constexpr uint8_t kTtyPong = 0x08;

inline constexpr uint8_t kTtyRoleListener = 1;
inline constexpr uint8_t kTtyRoleReceiver = 2;

inline constexpr uint8_t kTtyVersion = 1;
inline constexpr int kTtyHeaderSize = 4;
inline constexpr int kTtyMaxPayload = 16 * 1024;
inline constexpr char kTtyMagic[] = "WTTY";

struct TtyFrame {
  uint8_t type = 0;
  uint8_t flags = 0;
  std::vector<uint8_t> payload;
};

struct TtyHello {
  uint8_t version = kTtyVersion;
  uint8_t role = kTtyRoleListener;
  int cols = 80;
  int rows = 24;
};

const char* TtyTypeName(uint8_t t);

std::vector<uint8_t> TtyEncode(const TtyFrame& f);
bool TtyDecode(std::span<const uint8_t> p, TtyFrame* out);

std::vector<uint8_t> TtyEncodeHello(TtyHello h);
bool TtyDecodeHello(std::span<const uint8_t> p, TtyHello* out);

std::vector<uint8_t> TtyEncodeResize(int cols, int rows);
bool TtyDecodeResize(std::span<const uint8_t> p, int* cols, int* rows);

std::vector<uint8_t> TtyEncodeExit(int code);
bool TtyDecodeExit(std::span<const uint8_t> p, int* code);

class TtyDecoder {
 public:
  std::vector<TtyFrame> Push(std::span<const uint8_t> p);

 private:
  std::vector<uint8_t> buf_;
};

class TtySession {
 public:
  using SendFn = std::function<WowResult(std::span<const uint8_t>)>;
  using BytesFn = std::function<void(std::span<const uint8_t>)>;
  using HelloFn = std::function<void(const TtyHello&)>;
  using SizeFn = std::function<void(int, int)>;
  using ExitFn = std::function<void(int)>;
  using PingFn = std::function<void()>;

  void SetSend(SendFn fn) { send_ = std::move(fn); }

  void Push(std::span<const uint8_t> p);

  WowResult Send(const TtyFrame& f);
  WowResult WriteHello(TtyHello h);
  WowResult WriteStdin(std::span<const uint8_t> p);
  WowResult WriteStdout(std::span<const uint8_t> p);
  WowResult WriteStderr(std::span<const uint8_t> p);
  WowResult WriteResize(int cols, int rows);
  WowResult WriteExit(int code);
  WowResult WritePing();

  HelloFn OnHello;
  BytesFn OnStdin;
  BytesFn OnStdout;
  BytesFn OnStderr;
  SizeFn OnResize;
  ExitFn OnExit;
  PingFn OnPing;
  PingFn OnPong;

  const TtyHello& hello() const { return hello_; }
  bool pong() const { return pong_; }

 private:
  WowResult writeChunks(uint8_t typ, std::span<const uint8_t> p);
  void dispatch(const TtyFrame& f);

  TtyDecoder dec_;
  SendFn send_;
  TtyHello hello_;
  bool pong_ = false;
};

}  // namespace wow

#endif  // WOW_TTY_H_
