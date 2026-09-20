#ifndef WASMDROID_INCLUDE_DROID_BINDER_HOST_HPP_
#define WASMDROID_INCLUDE_DROID_BINDER_HOST_HPP_

// In-module Binder hop — Android's IPC analog of COM / RPC.
// Names follow uapi/linux/android/binder.h and libbinder
// (IBinder, Parcel, IServiceManager). Transactions stay on this hop.

#include "droid/catalog.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace wasmdroid {

enum { kBndBase = 12288 };
enum { kBndMax = 128 };
enum { kBndNode = 1, kBndParcel = 2, kBndAshmem = 3, kBndIon = 4 };

struct BndObj {
  int used = 0;
  int kind = 0;
  unsigned refs = 1;
  std::string name;
  std::string data;
  size_t size = 0;
};

inline BndObj* bnd_tab() {
  static BndObj t[kBndMax];
  return t;
}

inline std::map<std::string, int>& bnd_svcs() {
  static std::map<std::string, int> m;
  return m;
}

inline std::map<std::string, std::string>& bnd_props() {
  static std::map<std::string, std::string> m;
  return m;
}

inline bool bnd_eq(const char* a, const char* b) {
  return a && b && std::strcmp(a, b) == 0;
}

inline bool bnd_pfx(const char* a, const char* p) {
  return a && p && std::strncmp(a, p, std::strlen(p)) == 0;
}

inline void bnd_split(const char* a, std::string* l, std::string* r) {
  const char* p = a ? std::strchr(a, '\x1f') : nullptr;
  if (!p) {
    *l = a ? a : "";
    r->clear();
    return;
  }
  l->assign(a, p);
  *r = p + 1;
}

inline int bnd_alloc(int kind) {
  BndObj* t = bnd_tab();
  for (int i = 0; i < kBndMax; ++i) {
    if (!t[i].used) {
      t[i] = BndObj{};
      t[i].used = 1;
      t[i].kind = kind;
      t[i].refs = 1;
      return kBndBase + i;
    }
  }
  return -1;
}

inline BndObj* bnd_at(int h) {
  int i = h - kBndBase;
  if (i < 0 || i >= kBndMax || !bnd_tab()[i].used) return nullptr;
  return &bnd_tab()[i];
}

// Binder hop owns userspace IPC / ashmem / ion / HAL / properties / log.
// Kernel ashmem_* (ashmem_pin, ashmem_ioctl) stay on try_kernel via
// Android.Kernel.ashmem — do not prefix-match lowercase ashmem_ here.
inline bool bnd_ns(const char* ns) {
  return ns && (bnd_pfx(ns, "Android.Binder") || bnd_pfx(ns, "Android.Ashmem") ||
                bnd_pfx(ns, "Android.Ion") || bnd_pfx(ns, "Android.Dmabuf") ||
                bnd_pfx(ns, "Android.HAL") || bnd_pfx(ns, "Android.Hwbinder") ||
                bnd_pfx(ns, "Android.HIDL") || bnd_pfx(ns, "Android.AIDL") ||
                bnd_pfx(ns, "Android.NDK") || bnd_pfx(ns, "Android.Keystore") ||
                bnd_pfx(ns, "Android.Service") ||
                bnd_eq(ns, "Android.Bionic.log") ||
                bnd_eq(ns, "Android.Bionic.property"));
}

inline bool bnd_cataloged(const char* api) {
  if (!api || !api[0]) return false;
  int n = 0;
  const WasmDroidApi* c = wasmdroid_catalog(&n);
  for (int i = 0; i < n; ++i)
    if (c[i].name && bnd_eq(c[i].name, api) && bnd_ns(c[i].ns)) return true;
  return bnd_pfx(api, "BINDER_") || bnd_pfx(api, "BC_") || bnd_pfx(api, "BR_") ||
         bnd_pfx(api, "ASHMEM_") || bnd_pfx(api, "ION_") ||
         bnd_pfx(api, "__system_property") || bnd_pfx(api, "AIBinder_") ||
         bnd_pfx(api, "AParcel_") || bnd_pfx(api, "AServiceManager_");
}

inline bool try_binder(const char* api, const char* args, std::string* out) {
  if (!api) return false;
  if (!bnd_cataloged(api) &&
      !(bnd_pfx(api, "BINDER_") || bnd_pfx(api, "BC_") || bnd_pfx(api, "BR_") ||
        bnd_pfx(api, "ASHMEM_") || bnd_pfx(api, "ION_") ||
        bnd_pfx(api, "__system_property") || bnd_pfx(api, "property_") ||
        bnd_pfx(api, "__android_log") || bnd_pfx(api, "AIBinder_") ||
        bnd_pfx(api, "AParcel_") || bnd_pfx(api, "AServiceManager_")))
    return false;
  const char* a = args ? args : "";
  auto ok = [&](std::string s) {
    *out = std::move(s);
    return true;
  };
  auto fail = [&](const char* m) {
    *out = std::string("error: ") + m;
    return true;
  };

  if (bnd_eq(api, "BINDER_VERSION")) return ok("8");
  if (bnd_eq(api, "BINDER_SET_CONTEXT_MGR") || bnd_eq(api, "BINDER_SET_CONTEXT_MGR_EXT") ||
      bnd_eq(api, "BINDER_SET_MAX_THREADS") || bnd_eq(api, "BINDER_THREAD_EXIT") ||
      bnd_eq(api, "BINDER_SET_IDLE_TIMEOUT") || bnd_eq(api, "BINDER_SET_IDLE_PRIORITY") ||
      bnd_eq(api, "BINDER_ENABLE_ONEWAY_SPAM_DETECTION"))
    return ok("ok");
  if (bnd_eq(api, "BINDER_WRITE_READ") || bnd_eq(api, "BC_TRANSACTION") ||
      bnd_eq(api, "BC_REPLY") || bnd_eq(api, "AIBinder_transact")) {
    std::string svc, rest, code, payload;
    bnd_split(a, &svc, &rest);
    bnd_split(rest.c_str(), &code, &payload);
    auto it = bnd_svcs().find(svc);
    if (it == bnd_svcs().end() && !svc.empty()) {
      // still complete the hop; unknown service is a dead reply
      if (bnd_eq(api, "BC_TRANSACTION") || bnd_eq(api, "AIBinder_transact"))
        return ok("BR_DEAD_REPLY");
    }
    std::string reply = payload.empty() ? "ok" : payload;
    return ok(std::string("BR_REPLY") + "\x1f" + reply);
  }
  if (bnd_pfx(api, "BC_") || bnd_pfx(api, "BR_")) {
    if (bnd_eq(api, "BR_OK") || bnd_eq(api, "BC_ENTER_LOOPER") ||
        bnd_eq(api, "BC_REGISTER_LOOPER") || bnd_eq(api, "BC_EXIT_LOOPER") ||
        bnd_eq(api, "BC_ACQUIRE") || bnd_eq(api, "BC_RELEASE") ||
        bnd_eq(api, "BC_INCREFS") || bnd_eq(api, "BC_DECREFS") ||
        bnd_eq(api, "BC_FREE_BUFFER"))
      return ok("ok");
    if (bnd_eq(api, "BR_NOOP")) return ok("ok");
    if (bnd_eq(api, "BR_SPAWN_LOOPER")) return ok("ok");
    return ok(api);
  }

  if (bnd_eq(api, "defaultServiceManager") || bnd_eq(api, "ProcessState") ||
      bnd_eq(api, "IPCThreadState") || bnd_eq(api, "AServiceManager_addService") ||
      bnd_eq(api, "IServiceManager_addService")) {
    if (bnd_eq(api, "AServiceManager_addService") ||
        bnd_eq(api, "IServiceManager_addService")) {
      std::string name, rest;
      bnd_split(a, &name, &rest);
      if (name.empty()) return fail(api);
      int h = bnd_alloc(kBndNode);
      if (h < 0) return fail(api);
      bnd_at(h)->name = name;
      bnd_svcs()[name] = h;
      return ok(std::to_string(h));
    }
    int h = bnd_alloc(kBndNode);
    if (h < 0) return fail(api);
    bnd_at(h)->name = "servicemanager";
    bnd_svcs()["servicemanager"] = h;
    return ok(std::to_string(h));
  }
  if (bnd_eq(api, "AServiceManager_getService") || bnd_eq(api, "AServiceManager_checkService") ||
      bnd_eq(api, "IServiceManager_getService") || bnd_eq(api, "IServiceManager_checkService")) {
    if (!a[0]) return fail(api);
    auto it = bnd_svcs().find(a);
    if (it == bnd_svcs().end()) {
      if (bnd_eq(api, "AServiceManager_checkService") ||
          bnd_eq(api, "IServiceManager_checkService"))
        return ok("0");
      return fail("service not found");
    }
    return ok(std::to_string(it->second));
  }
  if (bnd_eq(api, "AServiceManager_listServices") || bnd_eq(api, "IServiceManager_listServices")) {
    std::string s;
    for (const auto& kv : bnd_svcs()) {
      if (!s.empty()) s += "\x1f";
      s += kv.first;
    }
    return ok(s);
  }
  if (bnd_eq(api, "AIBinder_incStrong") || bnd_eq(api, "IBinder_incStrong") ||
      bnd_eq(api, "AIBinder_decStrong") || bnd_eq(api, "IBinder_decStrong") ||
      bnd_eq(api, "AIBinder_linkToDeath") || bnd_eq(api, "AIBinder_unlinkToDeath")) {
    BndObj* o = bnd_at(static_cast<int>(std::strtol(a, nullptr, 10)));
    if (!o) return fail(api);
    if (bnd_eq(api, "AIBinder_incStrong") || bnd_eq(api, "IBinder_incStrong")) o->refs++;
    if (bnd_eq(api, "AIBinder_decStrong") || bnd_eq(api, "IBinder_decStrong")) {
      if (o->refs) o->refs--;
    }
    return ok("ok");
  }
  if (bnd_eq(api, "AParcel_create") || bnd_eq(api, "Parcel")) {
    int h = bnd_alloc(kBndParcel);
    if (h < 0) return fail(api);
    return ok(std::to_string(h));
  }
  if (bnd_eq(api, "AParcel_writeString") || bnd_eq(api, "AParcel_writeInt32") ||
      bnd_eq(api, "Parcel_writeString") || bnd_eq(api, "Parcel_writeInt32")) {
    std::string hs, val;
    bnd_split(a, &hs, &val);
    BndObj* o = bnd_at(static_cast<int>(std::strtol(hs.c_str(), nullptr, 10)));
    if (!o || o->kind != kBndParcel) return fail(api);
    if (!o->data.empty()) o->data += "\x1f";
    o->data += val;
    return ok("ok");
  }
  if (bnd_eq(api, "AParcel_readString") || bnd_eq(api, "AParcel_readInt32") ||
      bnd_eq(api, "Parcel_readString")) {
    BndObj* o = bnd_at(static_cast<int>(std::strtol(a, nullptr, 10)));
    if (!o) return fail(api);
    return ok(o->data);
  }

  if (bnd_eq(api, "__system_property_get") || bnd_eq(api, "property_get")) {
    if (!a[0]) return fail(api);
    auto it = bnd_props().find(a);
    if (it != bnd_props().end()) return ok(it->second);
    if (const char* e = std::getenv(a)) return ok(e);
    if (bnd_eq(a, "ro.build.version.sdk")) return ok("34");
    if (bnd_eq(a, "ro.product.cpu.abi") || bnd_eq(a, "ro.product.cpu.abilist"))
      return ok("x86_64");
    if (bnd_eq(a, "ro.build.version.release")) return ok("14");
    return fail("property not found");
  }
  if (bnd_eq(api, "__system_property_set") || bnd_eq(api, "property_set")) {
    std::string k, v;
    bnd_split(a, &k, &v);
    if (k.empty()) return fail(api);
    bnd_props()[k] = v;
    return ok("ok");
  }
  if (bnd_eq(api, "__system_property_find") || bnd_eq(api, "__system_property_read") ||
      bnd_eq(api, "__system_property_foreach") || bnd_eq(api, "property_list")) {
    if (bnd_eq(api, "property_list") || bnd_eq(api, "__system_property_foreach")) {
      std::string s;
      for (const auto& kv : bnd_props()) {
        if (!s.empty()) s += "\x1f";
        s += kv.first + "=" + kv.second;
      }
      return ok(s);
    }
    return try_binder("__system_property_get", a, out);
  }

  if (bnd_eq(api, "__android_log_print") || bnd_eq(api, "__android_log_write") ||
      bnd_eq(api, "__android_log_vprint") || bnd_eq(api, "__android_log_buf_write")) {
    if (a[0]) std::fputs(a, stderr);
    return ok("ok");
  }
  if (bnd_eq(api, "__android_log_is_loggable")) return ok("1");

  if (bnd_eq(api, "ASHMEM_SET_NAME") || bnd_eq(api, "ASHMEM_SET_SIZE") ||
      bnd_eq(api, "ashmem_create_region")) {
    std::string name, sz;
    bnd_split(a, &name, &sz);
    int h = bnd_alloc(kBndAshmem);
    if (h < 0) return fail(api);
    BndObj* o = bnd_at(h);
    o->name = name.empty() ? "ashmem" : name;
    o->size = static_cast<size_t>(std::strtoul(sz.empty() ? "4096" : sz.c_str(), nullptr, 10));
    if (!o->size) o->size = 4096;
    o->data.assign(o->size, '\0');
    return ok(std::to_string(h));
  }
  if (bnd_eq(api, "ASHMEM_GET_SIZE") || bnd_eq(api, "ASHMEM_GET_NAME") ||
      bnd_eq(api, "ashmem_get_size_region")) {
    BndObj* o = bnd_at(static_cast<int>(std::strtol(a, nullptr, 10)));
    if (!o) return fail(api);
    return ok(bnd_eq(api, "ASHMEM_GET_NAME") ? o->name : std::to_string(o->size));
  }
  if (bnd_eq(api, "ASHMEM_PIN") || bnd_eq(api, "ASHMEM_UNPIN") ||
      bnd_eq(api, "ASHMEM_SET_PROT_MASK") || bnd_eq(api, "ASHMEM_PURGE_ALL_CACHES") ||
      bnd_eq(api, "ashmem_set_prot_region") || bnd_eq(api, "ashmem_pin_region") ||
      bnd_eq(api, "ashmem_unpin_region"))
    return ok("ok");
  if (bnd_eq(api, "ION_IOC_ALLOC") || bnd_eq(api, "ION_IOC_FREE") ||
      bnd_eq(api, "ION_IOC_SHARE") || bnd_eq(api, "ION_IOC_MAP")) {
    if (bnd_eq(api, "ION_IOC_ALLOC")) {
      int h = bnd_alloc(kBndIon);
      if (h < 0) return fail(api);
      size_t n = static_cast<size_t>(std::strtoul(a[0] ? a : "4096", nullptr, 10));
      bnd_at(h)->size = n ? n : 4096;
      return ok(std::to_string(h));
    }
    return ok("ok");
  }

  if (bnd_eq(api, "hw_get_module") || bnd_eq(api, "hw_get_module_by_class") ||
      bnd_eq(api, "gralloc_open") || bnd_eq(api, "hwc_open_1")) {
    int h = bnd_alloc(kBndNode);
    if (h < 0) return fail(api);
    bnd_at(h)->name = a[0] ? a : api;
    return ok(std::to_string(h));
  }
  if (bnd_pfx(api, "gralloc_") || bnd_pfx(api, "hwc_") || bnd_pfx(api, "hw_"))
    return ok("ok");

  if (!bnd_cataloged(api)) return false;
  return ok("ok");
}

}  // namespace wasmdroid

#endif  // WASMDROID_INCLUDE_DROID_BINDER_HOST_HPP_
