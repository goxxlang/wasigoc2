#ifndef WASMDROID_INCLUDE_DROID_CATALOG_H_
#define WASMDROID_INCLUDE_DROID_CATALOG_H_

// Phone Link (the Windows Your Phone app). Not an Android libc.
#ifdef __cplusplus
extern "C" {
#endif

typedef struct WasmDroidApi {
  const char* ns;
  const char* lib;
  const char* name;
} WasmDroidApi;

const WasmDroidApi* wasmdroid_catalog(int* count);

#ifdef __cplusplus
}
#endif

#endif  // WASMDROID_INCLUDE_DROID_CATALOG_H_
