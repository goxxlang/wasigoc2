// wasigocvm feature bits — included early from runtime.hpp when
// WASIGO_GOCVM is set. Keeps product capability probes in one place.
#pragma once

#if defined(WASIGO_GOCVM) && WASIGO_GOCVM

#ifndef WASIGO_GOCVM_VERSION
#define WASIGO_GOCVM_VERSION 1
#endif

// Sockets: wasigocvm_net.hpp poll bridge. Fork/exec: std::thread child
// (WASIGO_GOCVM_HAS_PTHREAD when compiled -pthread).
#define WASIGO_GOCVM_HAS_SOCKETS 1

// mmap: wasi-libc ships sys/mman.h behind -D_WASI_EMULATED_MMAN and
// -lwasi-emulated-mman (MAP_ANON is a heap mapping in linear memory).
#ifndef _WASI_EMULATED_MMAN
#define _WASI_EMULATED_MMAN 1
#endif
#if defined(__has_include)
#  if __has_include(<sys/mman.h>)
#    define WASIGO_GOCVM_HAS_MMAN 1
#  endif
#endif
#ifndef WASIGO_GOCVM_HAS_MMAN
#define WASIGO_GOCVM_HAS_MMAN 0
#endif

#if defined(__has_include)
#  if __has_include(<pthread.h>) && defined(_REENTRANT)
#    define WASIGO_GOCVM_HAS_PTHREAD 1
#  endif
#endif
#ifndef WASIGO_GOCVM_HAS_PTHREAD
#define WASIGO_GOCVM_HAS_PTHREAD 0
#endif

// getpid: header comes from -D_WASI_EMULATED_GETPID. The symbol is
// gocvm's process table (wasigocvm_libc.hpp), not libwasi-emulated-getpid.
#ifndef _WASI_EMULATED_GETPID
#define _WASI_EMULATED_GETPID 1
#endif
#if defined(__has_include)
#  if __has_include(<unistd.h>)
#    define WASIGO_GOCVM_HAS_GETPID 1
#  endif
#endif
#ifndef WASIGO_GOCVM_HAS_GETPID
#define WASIGO_GOCVM_HAS_GETPID 0
#endif

// TLS: OpenSSL 3 via memory BIOs (WASMLime TlsTransport). Headers
// appear when compile.bat / wasigocvm.bat links toolchain/openssl-wasm (wasm
// libssl.a), not vcpkg's native mingw DLLs.
#if defined(__has_include)
#  if __has_include(<openssl/ssl.h>)
#    define WASIGO_GOCVM_HAS_OPENSSL 1
#  endif
#endif
#ifndef WASIGO_GOCVM_HAS_OPENSSL
#define WASIGO_GOCVM_HAS_OPENSSL 0
#endif

// Host ABI is the three catalogs. Always on — not a has_include probe.
#define WASIGO_HAS_WASMWIN32 1
#define WASIGO_HAS_WASMWIN32_CATALOG 1
#define WASIGO_HAS_WASMNIX 1
#define WASIGO_HAS_WASMNIX_CATALOG 1
#define WASIGO_HAS_WASMDROID 1
#define WASIGO_HAS_WASMDROID_CATALOG 1
#define WASIGO_HAS_WASMGOCOS 1
#define WASIGO_HAS_WASMGOCOS_CATALOG 1

#endif  // WASIGO_GOCVM
