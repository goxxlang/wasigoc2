#ifndef WNS_ADDR_H_
#define WNS_ADDR_H_

#include <cstdint>
#include <string>
#include <string_view>

namespace wns {

struct Ipv4 {
  uint8_t o[4]{};

  uint32_t ToU32() const {
    return (uint32_t(o[0]) << 24) | (uint32_t(o[1]) << 16) | (uint32_t(o[2]) << 8) |
           uint32_t(o[3]);
  }
  static Ipv4 FromU32(uint32_t v) {
    Ipv4 a;
    a.o[0] = uint8_t(v >> 24);
    a.o[1] = uint8_t(v >> 16);
    a.o[2] = uint8_t(v >> 8);
    a.o[3] = uint8_t(v);
    return a;
  }
  bool operator==(const Ipv4& x) const {
    return o[0] == x.o[0] && o[1] == x.o[1] && o[2] == x.o[2] && o[3] == x.o[3];
  }
  bool operator!=(const Ipv4& x) const { return !(*this == x); }
};

struct Ipv6 {
  uint8_t o[16]{};

  bool operator==(const Ipv6& x) const {
    for (int i = 0; i < 16; ++i) {
      if (o[i] != x.o[i]) return false;
    }
    return true;
  }
  bool operator!=(const Ipv6& x) const { return !(*this == x); }
};

bool ParseIpv4(std::string_view s, Ipv4* out);
bool ParseIpv6(std::string_view s, Ipv6* out);
std::string FormatIpv4(const Ipv4& a);
std::string FormatIpv6(const Ipv6& a);

bool Ipv4PrefixMatch(const Ipv4& addr, const Ipv4& net, uint8_t plen);
bool Ipv6PrefixMatch(const Ipv6& addr, const Ipv6& net, uint8_t plen);

bool IsLoopback4(const Ipv4& a);
bool IsLinkLocal4(const Ipv4& a);
bool IsPrivate4(const Ipv4& a);
bool IsCgnat4(const Ipv4& a);
bool IsReal4(const Ipv4& a);
bool IsInternet4(const Ipv4& a);

bool IsLoopback6(const Ipv6& a);
bool IsLinkLocal6(const Ipv6& a);
bool IsUla6(const Ipv6& a);
bool IsReal6(const Ipv6& a);
bool IsInternet6(const Ipv6& a);

}  // namespace wns

#endif  // WNS_ADDR_H_
