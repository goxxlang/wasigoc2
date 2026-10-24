#include "wns/ipify.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

bool SplitBind(const char* spec, std::string* host, uint16_t* port) {
  if (!spec || !host || !port) return false;
  std::string s(spec);
  if (!s.empty() && s.front() == '[') {
    auto rb = s.find(']');
    if (rb == std::string::npos) return false;
    *host = s.substr(1, rb - 1);
    if (rb + 1 < s.size() && s[rb + 1] == ':') {
      unsigned p = 0;
      if (std::sscanf(s.c_str() + rb + 2, "%u", &p) != 1 || p > 65535) return false;
      *port = uint16_t(p);
    }
    return true;
  }
  auto c = s.rfind(':');
  if (c == std::string::npos) {
    unsigned p = 0;
    if (std::sscanf(s.c_str(), "%u", &p) == 1 && p <= 65535) {
      *host = "0.0.0.0";
      *port = uint16_t(p);
      return true;
    }
    *host = s;
    return true;
  }
  *host = s.substr(0, c);
  unsigned p = 0;
  if (std::sscanf(s.c_str() + c + 1, "%u", &p) != 1 || p > 65535) return false;
  *port = uint16_t(p);
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  std::string host = "0.0.0.0";
  uint16_t port = 1790;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--bind") == 0 && i + 1 < argc) {
      if (!SplitBind(argv[++i], &host, &port)) {
        std::fprintf(stderr, "bad --bind\n");
        return 1;
      }
    } else if (!SplitBind(argv[i], &host, &port)) {
      std::fprintf(stderr, "usage: wns_ipify [--bind host:port]\n");
      return 1;
    }
  }
  wns::IpifyServer s;
  if (!s.Listen(host.c_str(), port)) {
    std::fprintf(stderr, "listen failed on %s:%u\n", host.c_str(), port);
    return 1;
  }
  std::printf("wns ipify on http://%s:%u/?format=json\n", host.c_str(), s.port());
  std::printf("client: GET /ipify.js\n");
  std::fflush(stdout);
  s.Serve();
  return 0;
}
