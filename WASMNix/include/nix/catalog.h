#ifndef WASMNIX_INCLUDE_NIX_CATALOG_H_
#define WASMNIX_INCLUDE_NIX_CATALOG_H_

// WSL. Not a man-page list.
#ifdef __cplusplus
extern "C" {
#endif

typedef struct WasmNixApi {
  const char* ns;
  const char* lib;
  const char* name;
} WasmNixApi;

const WasmNixApi* wasmnix_catalog(int* count);

#ifdef __cplusplus
}
#endif

#endif  // WASMNIX_INCLUDE_NIX_CATALOG_H_
