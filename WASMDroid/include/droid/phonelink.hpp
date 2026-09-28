#ifndef WASMDROID_INCLUDE_DROID_PHONELINK_HPP_
#define WASMDROID_INCLUDE_DROID_PHONELINK_HPP_

// Phone Link (YourPhone). The PC side of a linked phone:
// search, read, write, and monitor SMS, notifications, photos, calls, and apps.
// Package family: Microsoft.YourPhone_8wekyb3d8bbwe
// AUMID:          Microsoft.YourPhone_8wekyb3d8bbwe!App
// Protocol:       ms-phone:

#include "droid/catalog.h"
#include "droid/dispatch.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace wasmdroid {

inline bool eq(const char* a, const char* b) {
  return a && b && std::strcmp(a, b) == 0;
}

inline std::string err_msg(const char* msg) { return std::string("error: ") + msg; }

inline void split1f(const char* a, std::string* left, std::string* right) {
  const char* p = a ? std::strchr(a, '\x1f') : nullptr;
  if (!p) {
    *left = a ? a : "";
    right->clear();
    return;
  }
  left->assign(a, p);
  *right = p + 1;
}

struct PhoneItem {
  std::string id;
  std::string kind;
  std::string peer;
  std::string body;
  bool seen = false;
};

struct PhoneLink {
  std::string activation;
  std::vector<PhoneItem> items;
  std::uint64_t next = 1;
};

inline PhoneLink& phone() {
  static PhoneLink link;
  return link;
}

inline bool linked() { return !phone().activation.empty(); }

inline std::string item_row(const PhoneItem& it) {
  return it.id + "\x1f" + it.kind + "\x1f" + it.peer + "\x1f" + it.body;
}

inline std::string join_rows(const std::vector<const PhoneItem*>& rows) {
  std::string out;
  for (const PhoneItem* it : rows) {
    if (!out.empty()) out.push_back('\n');
    out += item_row(*it);
  }
  return out;
}

inline bool kind_ok(const std::string& k) {
  return k == "sms" || k == "notification" || k == "photo" || k == "call" || k == "app";
}

inline const PhoneItem* find_id(const std::string& id) {
  for (const PhoneItem& it : phone().items) {
    if (it.id == id) return &it;
  }
  return nullptr;
}

inline std::string list_kind(const std::string& kind) {
  std::vector<const PhoneItem*> rows;
  for (const PhoneItem& it : phone().items) {
    if (kind.empty() || it.kind == kind) rows.push_back(&it);
  }
  return join_rows(rows);
}

inline std::string phone_search(const std::string& q) {
  std::vector<const PhoneItem*> rows;
  for (const PhoneItem& it : phone().items) {
    if (q.empty() || it.kind.find(q) != std::string::npos || it.peer.find(q) != std::string::npos ||
        it.body.find(q) != std::string::npos || it.id == q) {
      rows.push_back(&it);
    }
  }
  return join_rows(rows);
}

inline std::string phone_read(const std::string& spec) {
  std::string kind, id;
  split1f(spec.c_str(), &kind, &id);
  if (id.empty()) {
    id = kind;
    kind.clear();
  }
  if (id.empty()) return list_kind(kind);
  const PhoneItem* it = find_id(id);
  if (!it || (!kind.empty() && it->kind != kind)) return err_msg("read: not found");
  return item_row(*it);
}

inline std::string phone_write(const std::string& spec) {
  std::string kind, rest, peer, body;
  split1f(spec.c_str(), &kind, &rest);
  split1f(rest.c_str(), &peer, &body);
  if (!kind_ok(kind) || peer.empty()) return err_msg("write: kind, peer, body");
  PhoneItem it;
  it.id = std::to_string(phone().next++);
  it.kind = kind;
  it.peer = peer;
  it.body = body;
  phone().items.push_back(std::move(it));
  return item_row(phone().items.back());
}

inline std::string phone_monitor(const std::string& kind) {
  std::vector<const PhoneItem*> rows;
  for (PhoneItem& it : phone().items) {
    if (it.seen) continue;
    if (!kind.empty() && it.kind != kind) continue;
    it.seen = true;
    rows.push_back(&it);
  }
  return join_rows(rows);
}

inline std::string phonelink_call(const char* api, const char* args) {
  if (!api || !api[0]) return err_msg("phonelink needs an API name");
  const char* a = args ? args : "";

  if (eq(api, "PackageFamilyName")) return "Microsoft.YourPhone_8wekyb3d8bbwe";
  if (eq(api, "Aumid")) return "Microsoft.YourPhone_8wekyb3d8bbwe!App";
  if (eq(api, "Protocol")) return "ms-phone:";
  if (eq(api, "Open")) {
    std::string uri = "ms-phone:";
    if (a[0]) {
      if (std::strncmp(a, "ms-phone:", 9) == 0)
        uri = a;
      else
        uri += a;
    }
    phone().activation = uri;
    return uri;
  }
  if (eq(api, "Status")) {
    if (!linked()) return "unlinked";
    return phone().activation;
  }

  const bool content = eq(api, "Search") || eq(api, "Read") || eq(api, "Write") || eq(api, "Monitor") ||
                       eq(api, "Sms") || eq(api, "Photos") || eq(api, "Notifications") || eq(api, "Calls") ||
                       eq(api, "Apps");
  if (content && !linked()) return err_msg("phonelink: unlinked");

  if (eq(api, "Search")) return phone_search(a);
  if (eq(api, "Read")) return phone_read(a);
  if (eq(api, "Write")) return phone_write(a);
  if (eq(api, "Monitor")) return phone_monitor(a);

  if (eq(api, "Sms")) {
    std::string op, rest;
    split1f(a, &op, &rest);
    if (op.empty() || op == "list") return list_kind("sms");
    if (op == "send") return phone_write(std::string("sms\x1f") + rest);
    if (op == "search") return phone_search(rest.empty() ? std::string("sms") : rest);
    return phone_read(std::string("sms\x1f") + op);
  }
  if (eq(api, "Photos")) return a[0] ? phone_read(std::string("photo\x1f") + a) : list_kind("photo");
  if (eq(api, "Notifications"))
    return a[0] ? phone_read(std::string("notification\x1f") + a) : list_kind("notification");
  if (eq(api, "Calls")) return a[0] ? phone_read(std::string("call\x1f") + a) : list_kind("call");
  if (eq(api, "Apps")) return a[0] ? phone_read(std::string("app\x1f") + a) : list_kind("app");

  return std::string("error: unknown api ") + api;
}

inline int fill_out(char* out, unsigned cap, const std::string& s) {
  if (!out || cap == 0) return s.rfind("error:", 0) == 0 ? -1 : 0;
  unsigned n = static_cast<unsigned>(s.size());
  if (n + 1 > cap) n = cap - 1;
  std::memcpy(out, s.data(), n);
  out[n] = 0;
  return s.rfind("error:", 0) == 0 ? -1 : 0;
}

}  // namespace wasmdroid

#endif  // WASMDROID_INCLUDE_DROID_PHONELINK_HPP_
