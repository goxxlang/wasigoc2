#ifndef WASMDROID_INCLUDE_DROID_CATALOG_H_
#define WASMDROID_INCLUDE_DROID_CATALOG_H_

// Curated Android.Bionic.* / Android.Binder.* / Android.Kernel.* rows
// from AOSP (bionic, libbinder, uapi/linux/android/binder.h) plus the
// Android kernel EXPORT_SYMBOL surface and hypervisor hop (KVM / pKVM /
// Gunyah / AVF). Not a dump of android.jar or of every HIDL interface.
#ifdef __cplusplus
extern "C" {
#endif

typedef struct WasmDroidApi {
  const char* ns;   // Android.Bionic, Android.Binder, Android.Kernel, ...
  const char* lib;  // libc, libbinder, liblog, vmlinux, libkvm, ...
  const char* name; // Bionic / Binder / ioctl / EXPORT_SYMBOL / CLI
} WasmDroidApi;

const WasmDroidApi* wasmdroid_catalog(int* count);

#ifdef __cplusplus
}
#endif

#endif  // WASMDROID_INCLUDE_DROID_CATALOG_H_
