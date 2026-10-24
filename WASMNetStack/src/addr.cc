#include "wns/addr.h"

#include <cstdio>
#include <string>

namespace wns {
namespace {

int HexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

bool SplitGroups(std::string_view s, uint16_t* g, int* n) {
  *n = 0;
  if (s.empty()) return true;
  size_t start = 0;
  for (size_t i = 0; i <= s.size(); ++i) {
    if (i != s.size() && s[i] != ':') continue;
    if (i == start) return false;
    if (*n >= 8) return false;
    if (i - start > 4) return false;
    unsigned v = 0;
    for (size_t k = start; k < i; ++k) {
      int h = HexVal(s[k]);
      if (h < 0) return false;
      v = (v << 4) | unsigned(h);
      if (v > 0xffff) return false;
    }
    g[(*n)++] = uint16_t(v);
    start = i + 1;
  }
  return true;
}

}  // namespace

bool ParseIpv4(std::string_view s, Ipv4* out) {
  if (!out || s.empty()) return false;
  unsigned a = 0, b = 0, c = 0, d = 0;
  char extra = 0;
  std::string tmp(s);
  if (std::sscanf(tmp.c_str(), "%u.%u.%u.%u%c", &a, &b, &c, &d, &extra) != 4) {
    return false;
  }
  if (a > 255 || b > 255 || c > 255 || d > 255) return false;
  out->o[0] = uint8_t(a);
  out->o[1] = uint8_t(b);
  out->o[2] = uint8_t(c);
  out->o[3] = uint8_t(d);
  return true;
}

bool ParseIpv6(std::string_view s, Ipv6* out) {
  if (!out || s.empty()) return false;
  std::string t(s);
  auto dc = t.find("::");
  uint16_t g[8] = {};
  if (dc == std::string::npos) {
    uint16_t groups[8];
    int n = 0;
    if (!SplitGroups(t, groups, &n) || n != 8) return false;
    for (int i = 0; i < 8; ++i) g[i] = groups[i];
  } else {
    if (t.find("::", dc + 2) != std::string::npos) return false;
    uint16_t left[8], right[8];
    int ln = 0, rn = 0;
    if (!SplitGroups(std::string_view(t).substr(0, dc), left, &ln)) return false;
    if (!SplitGroups(std::string_view(t).substr(dc + 2), right, &rn)) return false;
    if (ln + rn > 8) return false;
    for (int i = 0; i < ln; ++i) g[i] = left[i];
    for (int i = 0; i < rn; ++i) g[8 - rn + i] = right[i];
  }
  Ipv6 a{};
  for (int i = 0; i < 8; ++i) {
    a.o[i * 2] = uint8_t(g[i] >> 8);
    a.o[i * 2 + 1] = uint8_t(g[i]);
  }
  *out = a;
  return true;
}

std::string FormatIpv4(const Ipv4& a) {
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%u.%u.%u.%u", a.o[0], a.o[1], a.o[2], a.o[3]);
  return buf;
}

std::string FormatIpv6(const Ipv6& a) {
  char fmt[48];
  std::snprintf(fmt, sizeof(fmt), "%x:%x:%x:%x:%x:%x:%x:%x",
                (unsigned(a.o[0]) << 8) | a.o[1], (unsigned(a.o[2]) << 8) | a.o[3],
                (unsigned(a.o[4]) << 8) | a.o[5], (unsigned(a.o[6]) << 8) | a.o[7],
                (unsigned(a.o[8]) << 8) | a.o[9], (unsigned(a.o[10]) << 8) | a.o[11],
                (unsigned(a.o[12]) << 8) | a.o[13], (unsigned(a.o[14]) << 8) | a.o[15]);
  return fmt;
}

bool Ipv4PrefixMatch(const Ipv4& addr, const Ipv4& net, uint8_t plen) {
  if (plen > 32) return false;
  uint32_t mask = plen == 0 ? 0 : (~uint32_t(0) << (32 - plen));
  return (addr.ToU32() & mask) == (net.ToU32() & mask);
}

bool Ipv6PrefixMatch(const Ipv6& addr, const Ipv6& net, uint8_t plen) {
  if (plen > 128) return false;
  int full = plen / 8;
  int rem = plen % 8;
  for (int i = 0; i < full; ++i) {
    if (addr.o[i] != net.o[i]) return false;
  }
  if (rem) {
    uint8_t mask = uint8_t(0xFFu << (8 - rem));
    if ((addr.o[full] & mask) != (net.o[full] & mask)) return false;
  }
  return true;
}

bool IsLoopback4(const Ipv4& a) { return a.o[0] == 127; }

bool IsLinkLocal4(const Ipv4& a) { return a.o[0] == 169 && a.o[1] == 254; }

bool IsPrivate4(const Ipv4& a) {
  if (a.o[0] == 10) return true;
  if (a.o[0] == 192 && a.o[1] == 168) return true;
  if (a.o[0] == 172 && a.o[1] >= 16 && a.o[1] <= 31) return true;
  return false;
}

bool IsCgnat4(const Ipv4& a) { return a.o[0] == 100 && (a.o[1] & 0xc0) == 64; }

bool IsReal4(const Ipv4& a) {
  return IsLoopback4(a) || IsLinkLocal4(a) || IsPrivate4(a) || IsCgnat4(a);
}

bool IsInternet4(const Ipv4& a) {
  if (a.ToU32() == 0 || a.ToU32() == 0xffffffffu) return false;
  if ((a.o[0] & 0xf0) == 0xe0) return false;
  if (IsReal4(a)) return false;
  return true;
}

bool IsLoopback6(const Ipv6& a) {
  for (int i = 0; i < 15; ++i) {
    if (a.o[i] != 0) return false;
  }
  return a.o[15] == 1;
}

bool IsLinkLocal6(const Ipv6& a) { return a.o[0] == 0xfe && (a.o[1] & 0xc0) == 0x80; }

bool IsUla6(const Ipv6& a) { return (a.o[0] & 0xfe) == 0xfc; }

bool IsReal6(const Ipv6& a) { return IsLoopback6(a) || IsLinkLocal6(a) || IsUla6(a); }

bool IsInternet6(const Ipv6& a) {
  bool unspecified = true;
  for (int i = 0; i < 16; ++i) {
    if (a.o[i] != 0) unspecified = false;
  }
  if (unspecified) return false;
  if (a.o[0] == 0xff) return false;
  if (IsReal6(a)) return false;
  return true;
}

}  // namespace wns
