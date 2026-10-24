#include "wns/assoc.h"

#include "wns/sctp.h"

namespace wns {

uint32_t MakeVtag(uint32_t node_id) {
  return node_id * 0x9e3779b9u + 1u;
}

uint16_t Association::NextSsn(uint16_t sid) {
  for (auto& p : ssn) {
    if (p.first == sid) return p.second++;
  }
  ssn.push_back({sid, 1});
  return 0;
}

bool Association::AddPath4(Ipv4 a, bool primary) {
  Path p;
  p.v4 = a;
  p.v6 = false;
  p.active = true;
  p.primary = primary;
  if (primary) {
    for (auto& x : paths) x.primary = false;
  } else if (paths.empty()) {
    p.primary = true;
  }
  paths.push_back(p);
  return true;
}

bool Association::AddPath6(Ipv6 a, bool primary) {
  Path p;
  p.ipv6 = a;
  p.v6 = true;
  p.active = true;
  p.primary = primary;
  if (primary) {
    for (auto& x : paths) x.primary = false;
  } else if (paths.empty()) {
    p.primary = true;
  }
  paths.push_back(p);
  return true;
}

bool Association::FailPrimary() {
  Path* prim = nullptr;
  Path* backup = nullptr;
  for (auto& p : paths) {
    if (p.primary) prim = &p;
    else if (p.active && !backup) backup = &p;
  }
  if (!prim || !backup) return false;
  prim->active = false;
  prim->primary = false;
  backup->primary = true;
  return true;
}

const Path* Association::Primary() const {
  for (const auto& p : paths) {
    if (p.primary && p.active) return &p;
  }
  for (const auto& p : paths) {
    if (p.active) return &p;
  }
  return nullptr;
}

}  // namespace wns
