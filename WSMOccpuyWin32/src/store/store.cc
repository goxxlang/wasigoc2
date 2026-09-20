#include "wow/store.h"

namespace wow {

std::string Channel::ToJson() const {
  std::string o = "{\"id\":";
  o += std::to_string(id);
  o += ",\"kind\":";
  o += JsonEscape(KindToString(kind));
  o += ",\"isolation\":";
  o += JsonEscape(IsolationToString(isolation));
  o += ",\"thin\":";
  o += IsThin(kind) ? "true" : "false";
  o += ",\"label\":";
  o += JsonEscape(label);
  o += ",\"bytes\":";
  o += std::to_string(bytes.size());
  o += ",\"writes\":";
  o += std::to_string(writes);
  o += ",\"reads\":";
  o += std::to_string(reads);
  o += ",\"submits\":";
  o += std::to_string(submits);
  o += ",\"mapped\":";
  o += mapped ? "true" : "false";
  o += ",\"live\":";
  o += live ? "true" : "false";
  o += ",\"parent\":";
  o += std::to_string(parent);
  o += '}';
  return o;
}

OccupancyTable::OccupancyTable() { Clear(); }

void OccupancyTable::Clear() {
  channels_.clear();
  next_id_ = 1;
}

Channel OccupancyTable::Open(SurfaceKind kind, std::string_view label,
                             int parent) {
  Channel c;
  c.id = next_id_++;
  c.kind = kind;
  c.isolation = IsolationOf(kind);
  c.label = label.empty() ? KindToString(kind) : std::string(label);
  c.parent = parent;
  channels_.push_back(c);
  return channels_.back();
}

std::optional<Channel> OccupancyTable::Find(int id) const {
  for (const auto& c : channels_) {
    if (c.id == id) {
      return c;
    }
  }
  return std::nullopt;
}

Channel* OccupancyTable::FindMutable(int id) {
  for (auto& c : channels_) {
    if (c.id == id) {
      return &c;
    }
  }
  return nullptr;
}

std::vector<Channel> OccupancyTable::All() const { return channels_; }

std::vector<Channel> OccupancyTable::OfKind(SurfaceKind kind) const {
  std::vector<Channel> out;
  for (const auto& c : channels_) {
    if (c.kind == kind) {
      out.push_back(c);
    }
  }
  return out;
}

bool OccupancyTable::Drop(int id) {
  for (auto& c : channels_) {
    if (c.id == id) {
      c.live = false;
      c.mapped = false;
      return true;
    }
  }
  return false;
}

}  // namespace wow
