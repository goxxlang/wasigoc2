#include "wns/locator.h"

#include <cstddef>

namespace wns {
namespace {

template <typename T>
bool Contains(const std::vector<T>& v, const T& x) {
  for (const auto& e : v) {
    if (e == x) return true;
  }
  return false;
}

template <typename T>
bool Erase(std::vector<T>* v, const T& x) {
  for (size_t i = 0; i < v->size(); ++i) {
    if ((*v)[i] == x) {
      v->erase(v->begin() + static_cast<std::ptrdiff_t>(i));
      return true;
    }
  }
  return false;
}

}  // namespace

NodeLocators* LocatorTable::Find(uint32_t node_id) {
  for (auto& n : nodes_) {
    if (n.node_id == node_id) return &n;
  }
  return nullptr;
}

const NodeLocators* LocatorTable::Find(uint32_t node_id) const {
  for (const auto& n : nodes_) {
    if (n.node_id == node_id) return &n;
  }
  return nullptr;
}

NodeLocators& LocatorTable::Ensure(uint32_t node_id) {
  if (auto* n = Find(node_id)) return *n;
  NodeLocators n;
  n.node_id = node_id;
  nodes_.push_back(n);
  return nodes_.back();
}

bool LocatorTable::Add4(uint32_t node_id, Ipv4 a) {
  auto& n = Ensure(node_id);
  if (Contains(n.ipv4, a)) return false;
  n.ipv4.push_back(a);
  if (!n.has_primary4) {
    n.primary4 = a;
    n.has_primary4 = true;
  }
  return true;
}

bool LocatorTable::Add6(uint32_t node_id, Ipv6 a) {
  auto& n = Ensure(node_id);
  if (Contains(n.ipv6, a)) return false;
  n.ipv6.push_back(a);
  if (!n.has_primary6) {
    n.primary6 = a;
    n.has_primary6 = true;
  }
  return true;
}

bool LocatorTable::Delete4(uint32_t node_id, Ipv4 a) {
  auto* n = Find(node_id);
  if (!n) return false;
  if (!Erase(&n->ipv4, a)) return false;
  if (n->has_primary4 && n->primary4 == a) {
    if (n->ipv4.empty()) {
      n->has_primary4 = false;
      n->primary4 = Ipv4{};
    } else {
      n->primary4 = n->ipv4.front();
    }
  }
  return true;
}

bool LocatorTable::Delete6(uint32_t node_id, Ipv6 a) {
  auto* n = Find(node_id);
  if (!n) return false;
  if (!Erase(&n->ipv6, a)) return false;
  if (n->has_primary6 && n->primary6 == a) {
    if (n->ipv6.empty()) {
      n->has_primary6 = false;
      n->primary6 = Ipv6{};
    } else {
      n->primary6 = n->ipv6.front();
    }
  }
  return true;
}

bool LocatorTable::SetPrimary4(uint32_t node_id, Ipv4 a) {
  Add4(node_id, a);
  auto& n = Ensure(node_id);
  n.primary4 = a;
  n.has_primary4 = true;
  return true;
}

bool LocatorTable::SetPrimary6(uint32_t node_id, Ipv6 a) {
  Add6(node_id, a);
  auto& n = Ensure(node_id);
  n.primary6 = a;
  n.has_primary6 = true;
  return true;
}

bool LocatorTable::Replace4(uint32_t node_id, const std::vector<Ipv4>& addrs) {
  auto& n = Ensure(node_id);
  n.ipv4 = addrs;
  if (addrs.empty()) {
    n.has_primary4 = false;
    n.primary4 = Ipv4{};
  } else {
    n.primary4 = addrs.front();
    n.has_primary4 = true;
  }
  return true;
}

bool LocatorTable::Replace6(uint32_t node_id, const std::vector<Ipv6>& addrs) {
  auto& n = Ensure(node_id);
  n.ipv6 = addrs;
  if (addrs.empty()) {
    n.has_primary6 = false;
    n.primary6 = Ipv6{};
  } else {
    n.primary6 = addrs.front();
    n.has_primary6 = true;
  }
  return true;
}

void LocatorTable::SetSeq(uint32_t node_id, uint32_t seq) {
  Ensure(node_id).seq = seq;
}

void LocatorTable::SetPath(uint32_t node_id, ClassifyResult path) {
  Ensure(node_id).path = path;
}

void LocatorTable::SetScope(uint32_t node_id, RealInternet scope) {
  Ensure(node_id).scope = scope;
}

bool LocatorTable::AddReal4(uint32_t node_id, Ipv4 a) {
  auto& n = Ensure(node_id);
  if (Contains(n.real4, a)) return false;
  n.real4.push_back(a);
  return true;
}

bool LocatorTable::AddReal6(uint32_t node_id, Ipv6 a) {
  auto& n = Ensure(node_id);
  if (Contains(n.real6, a)) return false;
  n.real6.push_back(a);
  return true;
}

bool LocatorTable::AddInternet4(uint32_t node_id, Ipv4 a) {
  auto& n = Ensure(node_id);
  if (Contains(n.internet4, a)) return false;
  n.internet4.push_back(a);
  return true;
}

bool LocatorTable::AddInternet6(uint32_t node_id, Ipv6 a) {
  auto& n = Ensure(node_id);
  if (Contains(n.internet6, a)) return false;
  n.internet6.push_back(a);
  return true;
}

bool LocatorTable::ReplaceReal4(uint32_t node_id, const std::vector<Ipv4>& addrs) {
  Ensure(node_id).real4 = addrs;
  return true;
}

bool LocatorTable::ReplaceReal6(uint32_t node_id, const std::vector<Ipv6>& addrs) {
  Ensure(node_id).real6 = addrs;
  return true;
}

bool LocatorTable::ReplaceInternet4(uint32_t node_id, const std::vector<Ipv4>& addrs) {
  Ensure(node_id).internet4 = addrs;
  return true;
}

bool LocatorTable::ReplaceInternet6(uint32_t node_id, const std::vector<Ipv6>& addrs) {
  Ensure(node_id).internet6 = addrs;
  return true;
}

}  // namespace wns
