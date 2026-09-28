#ifndef WASMNIX_INCLUDE_NIX_DISPATCH_H_
#define WASMNIX_INCLUDE_NIX_DISPATCH_H_

// One call. WSL.
#ifdef __cplusplus
extern "C" {
#endif

int wasmnix_call(const char* api, const char* args, char* out, unsigned cap);

#ifdef __cplusplus
}
#endif

#endif  // WASMNIX_INCLUDE_NIX_DISPATCH_H_
