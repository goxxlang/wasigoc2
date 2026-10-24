#ifndef WNS_IPIFY_H_
#define WNS_IPIFY_H_

#include "wns/addr.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace wns {

// Own ipify client. Same fetch contract as the public one; this origin, not theirs.
inline constexpr const char kIpifyClientJs[] =
    "fetch(\"/?format=json\", { cache: \"no-store\" })"
    "  .then(response => response.json())"
    "  .then(data => console.log(\"Visible IP:\", data.ip))"
    "  .catch(error => console.error(\"Request failed:\", error));";

struct IpifyReply {
  std::string ip;
  bool v6 = false;
  Ipv4 v4{};
  Ipv6 ipv6{};
};

std::string EncodeIpifyJson(std::string_view ip);
bool DecodeIpifyJson(std::string_view json, IpifyReply* out);
bool ParseVisibleIp(std::string_view ip, IpifyReply* out);
std::string IpifyHttpJson(std::string_view ip);
std::string IpifyHttpJs();

class IpifyServer {
 public:
  IpifyServer() = default;
  ~IpifyServer();
  IpifyServer(const IpifyServer&) = delete;
  IpifyServer& operator=(const IpifyServer&) = delete;

  bool Listen(const char* host, uint16_t port);
  uint16_t port() const { return port_; }
  bool ServeOne();
  void Serve();
  void Close();

 private:
  std::uintptr_t listen_ = static_cast<std::uintptr_t>(-1);
  uint16_t port_ = 0;
  bool open_ = false;
};

}  // namespace wns

#endif  // WNS_IPIFY_H_
