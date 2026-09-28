#ifndef WASIGO_SYSROOT_WINDOWS_H_
#define WASIGO_SYSROOT_WINDOWS_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef long HRESULT;
typedef unsigned int UINT32;
typedef unsigned long long UINT64;
typedef unsigned short UINT16;
typedef int BOOL;

#ifndef S_OK
#define S_OK ((HRESULT)0)
#endif
#ifndef E_FAIL
#define E_FAIL ((HRESULT)0x80004005L)
#endif
#ifndef SUCCEEDED
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#endif
#ifndef FAILED
#define FAILED(hr) (((HRESULT)(hr)) < 0)
#endif

#ifndef _AMD64_
#define _AMD64_ 1
#endif
#ifndef _WIN64
#define _WIN64 1
#endif

#ifdef __cplusplus
}
#endif

#endif
