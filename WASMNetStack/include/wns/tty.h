#ifndef WNS_TTY_H_
#define WNS_TTY_H_

#include "wns/sctp.h"

#include <cstdint>
#include <functional>
#include <span>
#include <vector>

namespace wns {

// WTTY frame. Bytes match WASMTTY tty.Encode / tty.Decode.
struct TtyFrame {
  uint8_t type = 0;
  uint8_t flags = 0;
  std::vector<uint8_t> payload;
};

struct TtyHello {
  uint8_t version = sctp::kTtyVersion;
  uint8_t role = sctp::kTtyRoleListener;
  uint16_t cols = 80;
  uint16_t rows = 24;
};

std::vector<uint8_t> EncodeTtyFrame(const TtyFrame& f);
bool DecodeTtyFrame(std::span<const uint8_t> wire, TtyFrame* out);

std::vector<uint8_t> EncodeTtyHello(const TtyHello& h);
bool DecodeTtyHello(std::span<const uint8_t> wire, TtyHello* out);

std::vector<uint8_t> EncodeTtyResize(uint16_t cols, uint16_t rows);
bool DecodeTtyResize(std::span<const uint8_t> wire, uint16_t* cols, uint16_t* rows);

std::vector<uint8_t> EncodeTtyExit(int32_t code);
bool DecodeTtyExit(std::span<const uint8_t> wire, int32_t* code);

// Byte-stream reassembly, same rules as WASMTTY tty.Decoder.
class TtyDecoder {
 public:
  std::vector<TtyFrame> Push(std::span<const uint8_t> p);

 private:
  std::vector<uint8_t> buf_;
};

struct TtySession {
  uint16_t stream = 0;
  uint8_t role = 0;
  uint16_t cols = 0;
  uint16_t rows = 0;
  bool hello_sent = false;
  bool have_hello = false;
  bool exited = false;
  int32_t exit_code = 0;
  std::vector<uint8_t> stdin_bytes;
  std::vector<uint8_t> stdout_bytes;
  std::vector<uint8_t> stderr_bytes;
  uint32_t pings = 0;
  uint32_t pongs = 0;
  TtyDecoder dec;
};

// Carries WTTY frames on the TTY stream block. Does not own a PTY.
class TtyMux {
 public:
  using Sender = std::function<void(uint16_t stream, uint32_t ppid,
                                    std::span<const uint8_t> user, bool unordered)>;

  void SetSender(Sender s) { send_ = std::move(s); }
  void set_server(bool server) { server_ = server; }
  bool server() const { return server_; }

  // Open a listener session and send WTTY hello. Returns the stream, or 0.
  uint16_t Open(uint16_t cols, uint16_t rows);
  bool Stdin(uint16_t stream, std::span<const uint8_t> data);
  bool Stdout(uint16_t stream, std::span<const uint8_t> data);
  bool Stderr(uint16_t stream, std::span<const uint8_t> data);
  bool Resize(uint16_t stream, uint16_t cols, uint16_t rows);
  bool Exit(uint16_t stream, int32_t code);
  bool Ping(uint16_t stream);

  bool OnWire(std::span<const uint8_t> user, uint16_t sid);

  TtySession* Find(uint16_t stream);
  const TtySession* Find(uint16_t stream) const;

 private:
  uint16_t Alloc();
  TtySession* Adopt(uint16_t stream);
  bool Emit(uint16_t stream, const TtyFrame& f);
  bool WriteChunks(uint16_t stream, uint8_t type, std::span<const uint8_t> data);
  void Dispatch(TtySession* s, const TtyFrame& f);

  bool server_ = false;
  uint16_t next_ = 0;
  Sender send_;
  std::vector<TtySession> sessions_;
};

}  // namespace wns

#endif  // WNS_TTY_H_
