#include "wns/classify.h"

#include "wns/nat.h"

#include <cstdio>
#include <string>

namespace wns {
namespace {

bool ParseCidr(std::string_view cidr, Ipv4* a4, Ipv6* a6, uint8_t* plen, bool* v6) {
  auto slash = cidr.find('/');
  std::string_view addr = slash == std::string_view::npos ? cidr : cidr.substr(0, slash);
  int p = -1;
  if (slash != std::string_view::npos) {
    unsigned u = 0;
    std::string rest(cidr.substr(slash + 1));
    if (std::sscanf(rest.c_str(), "%u", &u) != 1) return false;
    p = int(u);
  }
  if (addr.find(':') != std::string_view::npos) {
    if (!ParseIpv6(addr, a6)) return false;
    *plen = p < 0 ? 128 : uint8_t(p);
    if (*plen > 128) return false;
    *v6 = true;
    return true;
  }
  if (!ParseIpv4(addr, a4)) return false;
  *plen = p < 0 ? 32 : uint8_t(p);
  if (*plen > 32) return false;
  *v6 = false;
  return true;
}

PathClass Stronger(PathClass a, PathClass b) {
  if (a == PathClass::kTor || b == PathClass::kTor) return PathClass::kTor;
  if (a == PathClass::kVpn || b == PathClass::kVpn) return PathClass::kVpn;
  if (a == PathClass::kClearnet || b == PathClass::kClearnet) return PathClass::kClearnet;
  return PathClass::kUnknown;
}

}  // namespace

void Classifier::AddTorExit4(Ipv4 a, uint8_t plen) {
  v4_.push_back({a, plen, PathClass::kTor});
}
void Classifier::AddTorExit6(Ipv6 a, uint8_t plen) {
  v6_.push_back({a, plen, PathClass::kTor});
}
void Classifier::AddVpnCidr4(Ipv4 net, uint8_t plen) {
  v4_.push_back({net, plen, PathClass::kVpn});
}
void Classifier::AddVpnCidr6(Ipv6 net, uint8_t plen) {
  v6_.push_back({net, plen, PathClass::kVpn});
}

bool Classifier::AddTorExit(std::string_view cidr) {
  Ipv4 a4{};
  Ipv6 a6{};
  uint8_t plen = 0;
  bool v6 = false;
  if (!ParseCidr(cidr, &a4, &a6, &plen, &v6)) return false;
  if (v6) {
    AddTorExit6(a6, plen);
  } else {
    AddTorExit4(a4, plen);
  }
  return true;
}

bool Classifier::AddVpnCidr(std::string_view cidr) {
  Ipv4 a4{};
  Ipv6 a6{};
  uint8_t plen = 0;
  bool v6 = false;
  if (!ParseCidr(cidr, &a4, &a6, &plen, &v6)) return false;
  if (v6) {
    AddVpnCidr6(a6, plen);
  } else {
    AddVpnCidr4(a4, plen);
  }
  return true;
}

PathClass Classifier::Longest4(const Ipv4& a) const {
  int best_len = -1;
  PathClass cls = PathClass::kUnknown;
  for (const auto& c : v4_) {
    if (!Ipv4PrefixMatch(a, c.net, c.plen)) continue;
    if (int(c.plen) > best_len) {
      best_len = int(c.plen);
      cls = c.cls;
    }
  }
  return cls;
}

PathClass Classifier::Longest6(const Ipv6& a) const {
  int best_len = -1;
  PathClass cls = PathClass::kUnknown;
  for (const auto& c : v6_) {
    if (!Ipv6PrefixMatch(a, c.net, c.plen)) continue;
    if (int(c.plen) > best_len) {
      best_len = int(c.plen);
      cls = c.cls;
    }
  }
  return cls;
}

PathClass Classifier::Lookup4(const Ipv4& a) const { return Longest4(a); }
PathClass Classifier::Lookup6(const Ipv6& a) const { return Longest6(a); }

ClassifyResult Classifier::Classify(const std::vector<Ipv4>& v4,
                                    const std::vector<Ipv6>& v6,
                                    const ClassifyResult* previous, uint32_t prev_seq,
                                    uint32_t seq) const {
  ClassifyResult r;
  for (const auto& a : v4) {
    auto c = Longest4(a);
    if (c == PathClass::kUnknown) c = PathClass::kClearnet;
    r.ipv4 = Stronger(r.ipv4, c);
  }
  for (const auto& a : v6) {
    auto c = Longest6(a);
    if (c == PathClass::kUnknown) c = PathClass::kClearnet;
    r.ipv6 = Stronger(r.ipv6, c);
  }
  if (v4.empty()) r.ipv4 = PathClass::kUnknown;
  if (v6.empty()) r.ipv6 = PathClass::kUnknown;

  r.path = Stronger(r.ipv4, r.ipv6);
  if (r.path == PathClass::kUnknown && (!v4.empty() || !v6.empty())) {
    r.path = PathClass::kClearnet;
  }

  if (r.ipv4 != PathClass::kUnknown && r.ipv6 != PathClass::kUnknown &&
      r.ipv4 != r.ipv6) {
    r.family_split = true;
    // Split-family is a VPN tell: tunnel on one stack, leak or native on the other.
    if (r.path != PathClass::kTor) r.path = PathClass::kVpn;
  }

  if (previous) {
    r.ipv6_dropped = previous->ipv6 != PathClass::kUnknown && v6.empty() && !v4.empty();
    if (r.ipv6_dropped && r.path != PathClass::kTor) r.path = PathClass::kVpn;
    if (prev_seq != 0 && seq == prev_seq + 1 && previous->path != PathClass::kUnknown) {
      r.rapid_hop = true;
      if (previous->path == PathClass::kTor || r.path == PathClass::kTor) {
        r.path = PathClass::kTor;
      }
    }
  }
  return r;
}

RealInternet Classifier::DetectRealInternet(const std::vector<Ipv4>& claimed4,
                                            const std::vector<Ipv6>& claimed6,
                                            const std::vector<Ipv4>& observed4,
                                            const std::vector<Ipv6>& observed6) {
  return NatStack::Scope(claimed4, claimed6, observed4, observed6);
}

}  // namespace wns
