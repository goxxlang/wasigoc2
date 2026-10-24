#include "wns/anycast.h"

#include <cctype>
#include <cstddef>
#include <cstdio>

namespace wns {
namespace {

const char* kCccSeeds[] = {
    "ns.shadow",
    "example.com",
    "www.example.com",
    "icannt.example.com",
    "bank.example.net",
    "login.example.org",
    "app.example.io",
    "phisheries.dev",
    "www.phisheries.dev",
};

std::string CanonName(std::string_view name) {
  std::string n(name);
  while (!n.empty() && (n.front() == '.' || n.front() == ' ')) n.erase(n.begin());
  while (!n.empty() && (n.back() == '.' || n.back() == ' ')) n.pop_back();
  for (char& c : n) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return n;
}

}  // namespace

Anycast::Anycast() { ParseIpv4(kDefaultVip, &vip_); }

void Anycast::Bind(Stack* local, Stack* peer) {
  local_ = local;
  peer_ = peer;
  if (local_ && peer_) local_->Connect(*peer_);
}

bool Anycast::SetVip(std::string_view ip) { return ParseIpv4(ip, &vip_); }

bool Anycast::ParseCidr4(std::string_view cidr, BgpPrefix4* out) const {
  if (!out) return false;
  auto slash = cidr.find('/');
  std::string_view addr = slash == std::string_view::npos ? cidr : cidr.substr(0, slash);
  if (!ParseIpv4(addr, &out->addr)) return false;
  out->plen = 32;
  if (slash != std::string_view::npos) {
    unsigned p = 0;
    std::string rest(cidr.substr(slash + 1));
    if (std::sscanf(rest.c_str(), "%u", &p) != 1 || p > 32) return false;
    out->plen = uint8_t(p);
  }
  return true;
}

void Anycast::InstallLocal(const BgpPrefix4& p, bool withdraw) {
  BgpUpdate u;
  if (withdraw) {
    u.withdrawn4.push_back(p);
  } else {
    u.nlri4.push_back(p);
  }
  if (local_) {
    if (!local_->bgp().configured()) local_->BgpOpen();
    local_->BgpAdvertise(u);
  }
}

bool Anycast::Advertise(std::string_view cidr, std::string_view nexthop) {
  (void)nexthop;
  BgpPrefix4 p;
  if (!ParseCidr4(cidr, &p)) return false;
  for (auto& e : advertised_) {
    if (e.addr == p.addr && e.plen == p.plen) return true;
  }
  advertised_.push_back(p);
  InstallLocal(p, false);
  return true;
}

bool Anycast::Withdraw(std::string_view cidr) {
  BgpPrefix4 p;
  if (!ParseCidr4(cidr, &p)) return false;
  for (size_t i = 0; i < advertised_.size(); ++i) {
    if (advertised_[i].addr == p.addr && advertised_[i].plen == p.plen) {
      advertised_.erase(advertised_.begin() + static_cast<std::ptrdiff_t>(i));
      break;
    }
  }
  InstallLocal(p, true);
  return true;
}

bool Anycast::Pin(std::string_view name) {
  std::string n = CanonName(name);
  if (n.empty()) return false;
  if (local_) local_->DnsPutA(n, vip_);
  if (peer_) peer_->DnsPutA(n, vip_);
  if (local_ && peer_) local_->DnsQuerySend(n, kDnsTypeA);
  for (const auto& e : pins_) {
    if (e == n) return true;
  }
  pins_.push_back(std::move(n));
  return true;
}

bool Anycast::Lookup(std::string_view name, Ipv4* out) const {
  if (!out) return false;
  std::string n = CanonName(name);
  for (const auto& e : pins_) {
    if (e == n) {
      *out = vip_;
      return true;
    }
  }
  return false;
}

void Anycast::SeedCCC() {
  for (auto* s : kCccSeeds) Pin(s);
}

}  // namespace wns
