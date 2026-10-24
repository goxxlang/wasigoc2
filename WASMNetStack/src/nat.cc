#include "wns/nat.h"

namespace wns {
namespace {

template <typename T>
bool Has(const std::vector<T>& v, const T& x) {
  for (const auto& e : v) {
    if (e == x) return true;
  }
  return false;
}

bool AnyReal4(const std::vector<Ipv4>& v) {
  for (const auto& a : v) {
    if (IsReal4(a)) return true;
  }
  return false;
}
bool AnyInet4(const std::vector<Ipv4>& v) {
  for (const auto& a : v) {
    if (IsInternet4(a)) return true;
  }
  return false;
}
bool AnyReal6(const std::vector<Ipv6>& v) {
  for (const auto& a : v) {
    if (IsReal6(a)) return true;
  }
  return false;
}
bool AnyInet6(const std::vector<Ipv6>& v) {
  for (const auto& a : v) {
    if (IsInternet6(a)) return true;
  }
  return false;
}

bool Same4(const std::vector<Ipv4>& a, const std::vector<Ipv4>& b) {
  for (const auto& x : a) {
    if (Has(b, x)) return true;
  }
  return false;
}
bool Same6(const std::vector<Ipv6>& a, const std::vector<Ipv6>& b) {
  for (const auto& x : a) {
    if (Has(b, x)) return true;
  }
  return false;
}

bool SameInternet(const std::vector<Ipv4>& c4, const std::vector<Ipv4>& o4,
                  const std::vector<Ipv6>& c6, const std::vector<Ipv6>& o6) {
  for (const auto& x : c4) {
    if (Has(o4, x) && IsInternet4(x)) return true;
  }
  for (const auto& x : c6) {
    if (Has(o6, x) && IsInternet6(x)) return true;
  }
  return false;
}

}  // namespace

RealInternet NatStack::Scope(const std::vector<Ipv4>& init4, const std::vector<Ipv6>& init6,
                             const std::vector<Ipv4>& wire4, const std::vector<Ipv6>& wire6) {
  RealInternet r;
  r.has_real = !init4.empty() || !init6.empty();
  bool claimed = r.has_real;
  bool observed = !wire4.empty() || !wire6.empty();
  bool same = Same4(init4, wire4) || Same6(init6, wire6);
  bool c_real = AnyReal4(init4) || AnyReal6(init6);
  bool c_inet = AnyInet4(init4) || AnyInet6(init6);
  bool o_real = AnyReal4(wire4) || AnyReal6(wire6);
  bool o_inet = AnyInet4(wire4) || AnyInet6(wire6);
  r.has_internet = o_inet || c_inet;

  if (!claimed && !observed) return r;
  if (same) {
    // INIT address is the one on the wire.
    r.scope = SameInternet(init4, wire4, init6, wire6) ? PathScope::kDirect : PathScope::kLan;
  } else if (claimed && observed) {
    if (c_real && o_inet) {
      // INIT private/CGNAT; wire source public.
      r.scope = PathScope::kNat;
      r.natted = true;
    } else if (c_real && o_real) {
      r.scope = PathScope::kLan;
    } else {
      r.scope = PathScope::kNat;
      r.natted = true;
    }
  } else if (claimed) {
    r.scope = (c_inet && !c_real) ? PathScope::kDirect : PathScope::kLan;
    r.has_internet = c_inet;
  } else {
    // ipify / observed public with no INIT real.
    r.scope = o_inet ? PathScope::kInternet : PathScope::kLan;
  }
  r.has_internet = r.has_internet || r.scope == PathScope::kDirect ||
                   r.scope == PathScope::kInternet || r.natted;
  return r;
}

}  // namespace wns
