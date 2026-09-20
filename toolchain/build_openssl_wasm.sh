#!/usr/bin/env bash
# Build the existing OpenSSL 3.6.3 tree (vcpkg / WASMLime stack) for
# wasigocvm wasm. Output: toolchain/openssl-wasm/{include,lib}.
# Does not rewrite TLS — wasigocvm_tls.hpp loads this libssl.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PREFIX="${WASIGO_OPENSSL_PREFIX:-$ROOT/toolchain/openssl-wasm}"
SRC="${OPENSSL_SRC:-}"
if [[ -z "$SRC" ]]; then
  for cand in \
    "${HOME}/vcpkg/buildtrees/openssl/src/nssl-3.6.3-28a9d34a24.clean" \
    "/mnt/c/Users/grego/vcpkg/buildtrees/openssl/src/nssl-3.6.3-28a9d34a24.clean"
  do
    if [[ -f "$cand/Configure" ]]; then SRC="$cand"; break; fi
  done
fi
if [[ -z "$SRC" || ! -f "$SRC/Configure" ]]; then
  echo "error: OpenSSL source not found; set OPENSSL_SRC" >&2
  exit 1
fi

win_path() {
  local p="$1"
  if [[ "$p" == /mnt/[a-zA-Z]/* ]]; then
    local drive
    drive="$(echo "${p:5:1}" | tr '[:lower:]' '[:upper:]')"
    echo "${drive}:/${p:7}"
    return
  fi
  echo "$p"
}

CLANG="${WASIGO_OPENSSL_CC:-}"
AR="${WASIGO_OPENSSL_AR:-}"
RANLIB="${WASIGO_OPENSSL_RANLIB:-}"
pick_bin() {
  local name="$1"
  for root in "${WASIGO_TOOLCHAIN:-}" "$ROOT/toolchain" "${WASI_SDK_PATH:-}" "${HOME}/wasi-sdk" \
              "/mnt/c/Users/grego/wasi-sdk" "/mnt/c/Users/grego/go++/toolchain"; do
    [[ -n "$root" ]] || continue
    for cand in "$root/bin/$name" "$root/bin/${name}.exe"; do
      if [[ -x "$cand" || -f "$cand" ]]; then echo "$cand"; return; fi
    done
  done
}
if [[ -z "$CLANG" ]]; then
  CLANG="$(pick_bin wasm32-wasigocvm-clang || true)"
  [[ -n "$CLANG" ]] || CLANG="$(pick_bin wasm32-wasip2-clang || true)"
  [[ -n "$CLANG" ]] || CLANG="$(pick_bin clang || true)"
fi
[[ -n "$CLANG" ]] || { echo "error: no wasi clang" >&2; exit 1; }
[[ -n "$AR" ]] || AR="$(pick_bin llvm-ar || true)"
[[ -n "$RANLIB" ]] || RANLIB="$(pick_bin llvm-ranlib || true)"
[[ -n "$AR" ]] || AR="llvm-ar"
[[ -n "$RANLIB" ]] || RANLIB="llvm-ranlib"

SYSROOT="${WASIGO_OPENSSL_SYSROOT:-}"
if [[ -z "$SYSROOT" ]]; then
  for cand in \
    "${WASIGO_TOOLCHAIN:-}/sysroot" \
    "$ROOT/toolchain/sysroot" \
    "${WASI_SDK_PATH:-}/share/wasi-sysroot" \
    "${HOME}/wasi-sdk/share/wasi-sysroot" \
    "/mnt/c/Users/grego/wasi-sdk/share/wasi-sysroot"
  do
    if [[ -d "$cand/include" ]]; then SYSROOT="$cand"; break; fi
  done
fi
if [[ "$CLANG" == *.exe ]]; then
  SYSROOT="$(win_path "$SYSROOT")"
fi

BUILD="$PREFIX/build"
mkdir -p "$BUILD" "$PREFIX"
export OPENSSL_LOCAL_CONFIG_DIR="$ROOT/toolchain/openssl-wasm"
export CROSS_COMPILE=""
export CC="$CLANG"
export AR
export RANLIB
CFLAGS="-O2 -D_WASI_EMULATED_GETPID -D_WASI_EMULATED_MMAN"
if [[ -n "${SYSROOT:-}" ]]; then
  CFLAGS="$CFLAGS --sysroot=$SYSROOT"
fi
export CFLAGS
export CPPFLAGS="${CPPFLAGS:-} -D_WASI_EMULATED_GETPID -D_WASI_EMULATED_MMAN"

echo "[openssl-wasm] src=$SRC"
echo "[openssl-wasm] cc=$CC"
echo "[openssl-wasm] sysroot=$SYSROOT"
echo "[openssl-wasm] prefix=$PREFIX"
cd "$BUILD"
perl "$SRC/Configure" \
  --prefix="$PREFIX" \
  --openssldir="$PREFIX/ssl" \
  --cross-compile-prefix="" \
  wasm32-wasigocvm \
  no-asm no-async no-egd no-ktls no-module no-secure-memory \
  no-shared no-sock no-thread-pool no-threads no-ui-console \
  no-weak-ssl-ciphers no-afalgeng no-dso no-engine no-tests \
  --with-rand-seed=getrandom

JOBS="${WASIGO_OPENSSL_JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}"
# opensslv.h and friends live in GENERATED_MANDATORY. build_libs also
# compiles apps/. Only the static libs TlsTransport loads.
make -j"$JOBS" build_generated
make -j"$JOBS" libcrypto.a libssl.a
mkdir -p "$PREFIX/lib" "$PREFIX/include"
cp -f libcrypto.a libssl.a "$PREFIX/lib/"
rm -rf "$PREFIX/include/openssl"
mkdir -p "$PREFIX/include/openssl"
cp -a "$SRC/include/openssl/." "$PREFIX/include/openssl/"
cp -a include/openssl/. "$PREFIX/include/openssl/"

echo
echo "wasm OpenSSL: $PREFIX/lib/libssl.a"
echo "headers:      $PREFIX/include/openssl/ssl.h"
