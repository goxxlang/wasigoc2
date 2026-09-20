#!/usr/bin/env bash
# wasigocvm.sh — Unix sibling of wasigocvm.bat
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
CONFIG="${WASIGO_CONFIG:-Release}"
GOFILE=""
OUTWASM=""
EXTRA=()

usage() {
  cat <<EOF
usage: wasigocvm.sh <file.go> [-o out.wasm]
  same machine as Windows compile.bat (the default)
  full libc++ / EH / RTTI, -DWASIGO_GOCVM=1
  prefers \$WASIGO_TOOLCHAIN or $ROOT/toolchain
  Isolation is WASMSafeSpace in this module. No companion gocvm_host.
  Stock wasip1 noeh leftover: legacy.bat
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    -h|--help) usage; exit 0 ;;
    -o) OUTWASM="$2"; shift 2 ;;
    Debug|Release) CONFIG="$1"; shift ;;
    *.go)
      [[ -n "$GOFILE" ]] && { echo "error: extra .go"; exit 2; }
      GOFILE="$1"; shift
      ;;
    *) EXTRA+=("$1"); shift ;;
  esac
done

find_wasigoc() {
  for c in \
    "$ROOT/build/$CONFIG/wasigoc" \
    "$ROOT/build/wasigoc" \
    "$ROOT/build-fork/wasigoc" \
    "$ROOT/wasigoc"
  do
    [[ -x "$c" ]] && { echo "$c"; return; }
  done
}

WASIGOC="$(find_wasigoc || true)"
if [[ -z "${WASIGOC:-}" ]]; then
  echo "error: wasigoc not found — build it first" >&2
  exit 1
fi
[[ -n "$GOFILE" ]] || { usage; exit 0; }
[[ -f "$GOFILE" ]] || { echo "error: not found: $GOFILE" >&2; exit 1; }

GO_ABS="$(cd "$(dirname "$GOFILE")" && pwd)/$(basename "$GOFILE")"
GO_DIR="$(dirname "$GO_ABS")"
GO_BASE="$(basename "$GO_ABS" .go)"
GENCPP="$GO_DIR/${GO_BASE}_gen.cpp"
OUTWASM="${OUTWASM:-$GO_DIR/${GO_BASE}.wasm}"

echo "[wasigoc] $GO_ABS"
"$WASIGOC" "$GO_ABS" -o "$GENCPP" --out-dir "$GO_DIR" "${EXTRA[@]+"${EXTRA[@]}"}"

# --- toolchain resolve ---
SYSROOT=""
CLANG=""
SDK_ROOT=""
if [[ -n "${WASIGO_TOOLCHAIN:-}" && -d "$WASIGO_TOOLCHAIN/sysroot/include" ]]; then
  SYSROOT="$WASIGO_TOOLCHAIN/sysroot"
  SDK_ROOT="$WASIGO_TOOLCHAIN"
elif [[ -d "$ROOT/toolchain/sysroot/include" ]]; then
  SYSROOT="$ROOT/toolchain/sysroot"
  SDK_ROOT="$ROOT/toolchain"
elif [[ -n "${WASI_SDK_PATH:-}" && -d "$WASI_SDK_PATH/share/wasi-sysroot/include" ]]; then
  SYSROOT="$WASI_SDK_PATH/share/wasi-sysroot"
  SDK_ROOT="$WASI_SDK_PATH"
elif [[ -d "${HOME}/wasi-sdk/share/wasi-sysroot/include" ]]; then
  SYSROOT="$HOME/wasi-sdk/share/wasi-sysroot"
  SDK_ROOT="$HOME/wasi-sdk"
fi

pick_clang() {
  local root="$1"
  for name in wasm32-wasigocvm-clang++ wasm32-wasip2-clang++; do
    [[ -x "$root/bin/$name" ]] && { echo "$root/bin/$name"; return; }
  done
}
[[ -n "$SDK_ROOT" ]] && CLANG="$(pick_clang "$SDK_ROOT" || true)"

# A toolchain/ install from bootstrap.sh's default (--target install) has a
# real sysroot but no bin/ of its own -- see toolchain/README.md. Pair that
# sysroot with the stock wasi-sdk's clang++ instead of giving up.
if [[ -z "${CLANG:-}" && -n "$SYSROOT" ]]; then
  for fallback_root in "${WASI_SDK_PATH:-}" "${HOME}/wasi-sdk"; do
    [[ -n "$fallback_root" ]] || continue
    CLANG="$(pick_clang "$fallback_root" || true)"
    [[ -n "$CLANG" ]] && { echo "[wasigocvm] pairing $SYSROOT with clang++ from $fallback_root (no bin/ of its own)"; break; }
  done
fi

if [[ -z "${CLANG:-}" || -z "${SYSROOT:-}" ]]; then
  echo "error: no wasigocvm clang/sysroot — run toolchain/bootstrap.sh or set WASIGO_TOOLCHAIN" >&2
  exit 1
fi

# Prefer wasigocvm layout, then stock wasip2 eh borrow.
TRIPLE=""
for t in wasm32-wasigocvm wasm32-wasip2; do
  if [[ -f "$SYSROOT/include/$t/eh/c++/v1/iostream" ]]; then
    TRIPLE="$t"
    break
  fi
done
if [[ -z "$TRIPLE" ]]; then
  echo "error: full libc++ (eh) missing under $SYSROOT/include" >&2
  exit 1
fi

EH_INC="$SYSROOT/include/$TRIPLE/eh/c++/v1"
EH_LIB="$SYSROOT/lib/$TRIPLE/eh"

WIN32_I=()
WIN32_CC=()
PE_I=()
if [[ -f "$ROOT/WASMPELoader/include/wasmpe/loader.hpp" ]]; then
  PE_I=(-I "$ROOT/WASMPELoader/include")
  echo "[wasigocvm] WASMPELoader"
elif [[ -f "${HOME}/WASMPELoader/include/wasmpe/loader.hpp" ]]; then
  PE_I=(-I "${HOME}/WASMPELoader/include")
  echo "[wasigocvm] WASMPELoader"
elif [[ -f "$ROOT/../WASMPELoader/include/wasmpe/loader.hpp" ]]; then
  PE_I=(-I "$ROOT/../WASMPELoader/include")
  echo "[wasigocvm] WASMPELoader"
fi
if [[ -f "$ROOT/WASMWin32/include/win32/wasi_host.hpp" ]]; then
  WIN32_I=(-I "$ROOT/WASMWin32/include")
  echo "[wasigocvm] WASMWin32 libc host"
  if [[ -f "$ROOT/WASMWin32/src/catalog.cc" ]]; then
    WIN32_CC=("$ROOT/WASMWin32/src/catalog.cc")
    echo "[wasigocvm] WASMWin32 catalog on EPT (TPT process/thread, CHPT session)"
  fi
  if [[ -f "$ROOT/WASMWin32/src/host_wasi.cc" ]]; then
    WIN32_CC+=("$ROOT/WASMWin32/src/host_wasi.cc")
    echo "[wasigocvm] WASMWin32 wasmwin32_call (occupyCalc CreateProcessW)"
  fi
fi

NIX_I=()
NIX_CC=()
if [[ -f "$ROOT/WASMNix/include/nix/posix_host.hpp" ]]; then
  NIX_I=(-I "$ROOT/WASMNix/include")
  echo "[wasigocvm] WASMNix libc host"
  if [[ -f "$ROOT/WASMNix/src/catalog.cc" ]]; then
    NIX_CC=("$ROOT/WASMNix/src/catalog.cc")
    echo "[wasigocvm] WASMNix catalog on EPT (CHPT session)"
  fi
fi

DROID_I=()
DROID_CC=()
if [[ -f "$ROOT/WASMDroid/include/droid/bionic_host.hpp" ]]; then
  DROID_I=(-I "$ROOT/WASMDroid/include")
  echo "[wasigocvm] WASMDroid libc host"
  if [[ -f "$ROOT/WASMDroid/src/catalog.cc" ]]; then
    DROID_CC=("$ROOT/WASMDroid/src/catalog.cc")
    echo "[wasigocvm] WASMDroid catalog on EPT (CHPT session, Binder root)"
  fi
fi
if [[ ${#WIN32_I[@]} -eq 0 ]]; then
  echo "error: wasigocvm requires WASMWin32/ in this repo (win32/wasi_host.hpp)" >&2
  exit 2
fi
if [[ ${#NIX_I[@]} -eq 0 ]]; then
  echo "error: wasigocvm requires WASMNix/ in this repo (nix/posix_host.hpp)" >&2
  exit 2
fi
if [[ ${#DROID_I[@]} -eq 0 ]]; then
  echo "error: wasigocvm requires WASMDroid/ in this repo (droid/bionic_host.hpp)" >&2
  exit 2
fi

GOCOS_I=()
GOCOS_CC=()
if [[ -f "$ROOT/WASMGocOS/include/gocos/host.hpp" ]]; then
  GOCOS_I=(-I "$ROOT/WASMGocOS/include")
  echo "[wasigocvm] WASMGocOS edge kernel"
  if [[ -f "$ROOT/WASMGocOS/src/catalog.cc" ]]; then
    GOCOS_CC=("$ROOT/WASMGocOS/src/catalog.cc")
    echo "[wasigocvm] WASMGocOS catalog on EPT (CHPT session, GocDesk)"
  fi
fi
if [[ ${#GOCOS_I[@]} -eq 0 ]]; then
  echo "error: wasigocvm requires WASMGocOS/ in this repo (gocos/host.hpp)" >&2
  exit 2
fi

WOW_I=()
WOW_CC=()
WOW_D=()
WOW_ROOT=""
if [[ -f "$ROOT/WSMOccpuyWin32/include/wow/occupancy.h" ]]; then
  WOW_ROOT="$ROOT/WSMOccpuyWin32"
elif [[ -f "${HOME}/WSMOccpuyWin32/include/wow/occupancy.h" ]]; then
  WOW_ROOT="${HOME}/WSMOccpuyWin32"
elif [[ -f "${HOME}/WSMOccupyWin32/include/wow/occupancy.h" ]]; then
  WOW_ROOT="${HOME}/WSMOccupyWin32"
elif [[ -f "$ROOT/../WSMOccpuyWin32/include/wow/occupancy.h" ]]; then
  WOW_ROOT="$ROOT/../WSMOccpuyWin32"
fi
if [[ -n "$WOW_ROOT" ]]; then
  WOW_I=(-I "$WOW_ROOT/include")
  WOW_D=(-DWOW_HAS_WIN32=1 -DWOW_HAS_GOCVM=1 -DWASIGO_GOCVM_BRIDGE=1)
  WOW_CC=(
    "$WOW_ROOT/src/catalog/catalog.cc"
    "$WOW_ROOT/src/surface/surface.cc"
    "$WOW_ROOT/src/store/store.cc"
    "$WOW_ROOT/src/event/event.cc"
    "$WOW_ROOT/src/occupancy/occupancy.cc"
    "$WOW_ROOT/src/ipc/ipc.cc"
    "$WOW_ROOT/src/harness/harness.cc"
    "$WOW_ROOT/src/gocvm/bridge.cc"
    "$WOW_ROOT/src/gocvm/install.cc"
    "$WOW_ROOT/src/c/system.cc"
    "$WOW_ROOT/src/driver/driver.cc"
    "$WOW_ROOT/src/posix/posix.cc"
    "$WOW_ROOT/src/tty/tty.cc"
  )
  echo "[wasigocvm] WSMOccpuyWin32 occupyCmd / CreateProcessW"
fi

SAFE_I=()
SAFE_CC=()
SAFE_ROOT=""
if [[ -f "$ROOT/WASMSafeSpace/src/sandbox/sandbox.cc" ]]; then
  SAFE_ROOT="$ROOT/WASMSafeSpace"
elif [[ -f "${HOME}/WASMSafeSpace/src/sandbox/sandbox.cc" ]]; then
  SAFE_ROOT="${HOME}/WASMSafeSpace"
elif [[ -f "$ROOT/../WASMSafeSpace/src/sandbox/sandbox.cc" ]]; then
  SAFE_ROOT="$ROOT/../WASMSafeSpace"
fi
if [[ -z "$SAFE_ROOT" ]]; then
  echo "error: wasigocvm requires WASMSafeSpace/ in this repo (sandbox.cc)" >&2
  exit 2
fi
if [[ -n "$SAFE_ROOT" ]]; then
  SAFE_I=(-I "$SAFE_ROOT/include" -I "$SAFE_ROOT")
  SAFE_CC=(
    "$SAFE_ROOT/src/sandbox/sandbox.cc"
    "$SAFE_ROOT/src/sandbox/external-pointer-table.cc"
    "$SAFE_ROOT/src/sandbox/trusted-pointer-table.cc"
  )
  echo "[wasigocvm] WASMSafeSpace EPT/TPT (exec second address space)"
fi

V8_I=()
V8_CC=()
V8_ROOT=""
if [[ -f "$ROOT/WASMv8bindings/include/cppgc/heap.h" ]]; then
  V8_ROOT="$ROOT/WASMv8bindings"
elif [[ -f "${HOME}/WASMv8bindings/include/cppgc/heap.h" ]]; then
  V8_ROOT="${HOME}/WASMv8bindings"
elif [[ -f "${HOME}/WASMv8Bindings/include/cppgc/heap.h" ]]; then
  V8_ROOT="${HOME}/WASMv8Bindings"
elif [[ -f "$ROOT/../WASMv8bindings/include/cppgc/heap.h" ]]; then
  V8_ROOT="$ROOT/../WASMv8bindings"
fi
if [[ -z "$V8_ROOT" ]]; then
  echo "error: wasigocvm requires WASMv8bindings/ in this repo (cppgc/heap.h)" >&2
  exit 2
fi
if [[ -n "$V8_ROOT" ]]; then
  V8_I=(-I "$V8_ROOT/include" -I "$V8_ROOT")
  V8_CC=(
    "$V8_ROOT/src/heap/internal/cppgc-logging.cc"
    "$V8_ROOT/src/heap/internal/free-list.cc"
    "$V8_ROOT/src/heap/internal/gc-info.cc"
    "$V8_ROOT/src/heap/internal/gc-info-table.cc"
    "$V8_ROOT/src/heap/internal/heap.cc"
    "$V8_ROOT/src/heap/internal/platform.cc"
    "$V8_ROOT/src/sandbox/cppheap-pointer-table.cc"
  )
  echo "[wasigocvm] WASMv8bindings CHPT (exec second address space)"
fi

SSL_I=()
SSL_L=()
SSL_D=()
if [[ -f "$ROOT/toolchain/openssl-wasm/include/openssl/ssl.h" && -f "$ROOT/toolchain/openssl-wasm/lib/libssl.a" ]]; then
  SSL_I=(-I "$ROOT/toolchain/openssl-wasm/include")
  SSL_L=("$ROOT/toolchain/openssl-wasm/lib/libssl.a" "$ROOT/toolchain/openssl-wasm/lib/libcrypto.a")
  SSL_D=(-DWASIGO_HAS_OPENSSL=1)
  echo "[wasigocvm] OpenSSL wasm (WASMLime TlsTransport / memory BIO)"
elif [[ -n "${SYSROOT:-}" && -n "${TRIPLE:-}" && -f "$SYSROOT/include/openssl/ssl.h" && -f "$SYSROOT/lib/$TRIPLE/libssl.a" ]]; then
  SSL_I=(-I "$SYSROOT/include")
  SSL_L=("$SYSROOT/lib/$TRIPLE/libssl.a" "$SYSROOT/lib/$TRIPLE/libcrypto.a")
  SSL_D=(-DWASIGO_HAS_OPENSSL=1)
  echo "[wasigocvm] OpenSSL wasm from sysroot"
fi

PTHREAD_FLAGS=()
STAMP="$ROOT/toolchain/wasigocvm-toolchain.json"
[[ -n "${SDK_ROOT:-}" && -f "$SDK_ROOT/wasigocvm-toolchain.json" ]] && STAMP="$SDK_ROOT/wasigocvm-toolchain.json"
if [[ -f "$STAMP" ]] && grep -Eq '"threads"[[:space:]]*:[[:space:]]*true' "$STAMP"; then
  PTHREAD_LIBDIR=""
  if [[ -f "$SYSROOT/lib/$TRIPLE/libpthread.a" ]]; then
    PTHREAD_LIBDIR="$SYSROOT/lib/$TRIPLE"
  elif [[ -f "${HOME}/wasi-sdk/share/wasi-sysroot/lib/$TRIPLE/libpthread.a" ]]; then
    PTHREAD_LIBDIR="${HOME}/wasi-sdk/share/wasi-sysroot/lib/$TRIPLE"
  elif [[ -f "$SYSROOT/lib/wasm32-wasip1-threads/libpthread.a" ]]; then
    PTHREAD_LIBDIR="$SYSROOT/lib/wasm32-wasip1-threads"
  fi
  if [[ -n "$PTHREAD_LIBDIR" ]]; then
    PTHREAD_FLAGS=(-D_REENTRANT -L "$PTHREAD_LIBDIR" -lpthread)
    echo "[wasigocvm] threads=on (std::thread fork, WASMSafeSpace cage, no wasm shared-memory)"
  else
    echo "warning: stamp threads:true but libpthread.a missing - building without -pthread" >&2
  fi
fi

echo "[wasigocvm] $CLANG ($TRIPLE)"
"$CLANG" -O2 -std=c++20 \
  -fexceptions -frtti \
  -fwasm-exceptions -mllvm -wasm-use-legacy-eh=false \
  -nostdinc++ \
  -isystem "$EH_INC" \
  -isystem "$SYSROOT/include/$TRIPLE" \
  -isystem "$SYSROOT/include" \
  -L "$EH_LIB" -lc++ -lc++abi -lunwind \
  -Wl,--export=__indirect_function_table \
  "${PTHREAD_FLAGS[@]}" \
  -I "$GO_DIR" -I "$ROOT/src" "${WIN32_I[@]}" "${NIX_I[@]}" "${DROID_I[@]}" "${GOCOS_I[@]}" "${WOW_I[@]}" "${PE_I[@]}" "${SAFE_I[@]}" "${V8_I[@]}" "${SSL_I[@]}" \
  -DWASIGO_GOCVM=1 "${WOW_D[@]}" "${SSL_D[@]}" -D_WASI_EMULATED_MMAN -D_WASI_EMULATED_GETPID \
  -lwasi-emulated-mman \
  -o "$OUTWASM" "$GENCPP" "${WIN32_CC[@]}" "${NIX_CC[@]}" "${DROID_CC[@]}" "${GOCOS_CC[@]}" "${WOW_CC[@]}" "${SAFE_CC[@]}" "${V8_CC[@]}" "${SSL_L[@]}"

echo
echo "wasm: $OUTWASM"
echo "run:  $ROOT/wasitime.sh $OUTWASM"
