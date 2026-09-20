#ifndef WASMDROID_INCLUDE_DROID_DISPATCH_H_
#define WASMDROID_INCLUDE_DROID_DISPATCH_H_

// WASMDroid projection entry: one call, named like Bionic / Binder /
// Android kernel uapi / AVF. Native Linux hits Bionic-shaped libc.
// Native Windows uses the in-module hop (optional adb). wasm32 maps
// getpid/getenv/uname/stat/clocks in-module so
// gocvm.Call("android"|"binder"|"kvm") stays in-module.
#ifdef __cplusplus
extern "C" {
#endif

// api: getpid, __system_property_get, BINDER_WRITE_READ, KVM_RUN, ...
// args: UTF-8, 0x1F-separated parameters (may be empty).
// out: UTF-8 reply, always NUL-terminated when cap > 0.
// returns 0 on success; nonzero is errno or -1 for unknown API.
int wasmdroid_call(const char* api, const char* args, char* out, unsigned cap);

#ifdef __cplusplus
}
#endif

#endif  // WASMDROID_INCLUDE_DROID_DISPATCH_H_
