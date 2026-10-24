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

# WASMReact (ReactOS kernel) and WASMProcess, when the siblings are there.
# Found before wasigoc runs so `import "reactos"` resolves.
REACT_ROOT=""
REACT_IPT=""
WPR_ROOT=""
for d in "${HOME}/WASMReact" "$ROOT/../WASMReact"; do
  [[ -f "$d/include/reactos/dispatch.h" ]] && { REACT_ROOT="$d"; break; }
done
if [[ -f "${HOME}/WASMVoodooCompile/WASMSafeSpace/src/sandbox/indirect-pointer-table.cc" ]]; then
  REACT_IPT="${HOME}/WASMVoodooCompile/WASMSafeSpace"
fi
for d in "${HOME}/WASMProcess" "$ROOT/../WASMProcess"; do
  [[ -f "$d/include/wpr/processes.h" ]] && { WPR_ROOT="$d"; break; }
done
REACT_I=()
REACT_D=()
REACT_CC=()
if [[ -n "$REACT_ROOT" && -n "$REACT_IPT" && -n "$WPR_ROOT" ]]; then
  REACT_I=(-I "$REACT_ROOT/include" -I "$REACT_IPT" -I "$WPR_ROOT/include" -I "$WPR_ROOT/src")
  REACT_D=(-DWASIGO_HAS_WASMREACT=1)
  REACT_CC=("$REACT_ROOT/src/unity.cc" "$REACT_IPT/src/sandbox/indirect-pointer-table.cc" "$REACT_ROOT/src/ps/drvfs.cc")
  EXTRA+=(--import-dir="$REACT_ROOT/go")
  echo "[wasigocvm] WASMReact ReactOS kernel on EPT/TPT/CHPT/IPT"
fi

# WASMWebGPU links only into a program that imports "webgpu": wasigoc
# writes stdlib_webgpu_gen.hpp then. A stale one from an earlier build
# must not count, so it goes first.
rm -f "$GO_DIR/stdlib_webgpu_gen.hpp"

echo "[wasigoc] $GO_ABS"
"$WASIGOC" "$GO_ABS" -o "$GENCPP" --out-dir "$GO_DIR" "${EXTRA[@]+"${EXTRA[@]}"}"

WEBGPU_I=()
WEBGPU_D=()
WEBGPU_CC=()
if [[ -f "$GO_DIR/stdlib_webgpu_gen.hpp" ]]; then
  if [[ ! -f "$ROOT/WASMWebGPU/src/catalog.cc" ]]; then
    echo "error: this program imports webgpu but WASMWebGPU/ is missing" >&2
    exit 1
  fi
  WEBGPU_I=(-I "$ROOT/WASMWebGPU/include")
  WEBGPU_D=(-DWASIGO_HAS_WEBGPU=1)
  WEBGPU_CC=("$ROOT/WASMWebGPU/src/catalog.cc" "$ROOT/WASMWebGPU/src/remote.cc")
  echo "            -I $ROOT/WASMWebGPU/include (webgpu catalog: import webgpu.batch)"
fi

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
    echo "[wasigocvm] WASMWin32 wasmwin32_call (CreateProcessW)"
  fi
fi

NIX_I=()
NIX_CC=()
if [[ -f "$ROOT/WASMNix/include/nix/posix_host.hpp" ]]; then
  NIX_I=(-I "$ROOT/WASMNix/include")
  echo "[wasigocvm] WASMNix libc host"
  if [[ -f "$ROOT/WASMNix/src/catalog.cc" ]]; then
    NIX_CC=("$ROOT/WASMNix/src/catalog.cc" "$ROOT/WASMNix/src/call.cc" "$ROOT/WASMNix/src/cage_wsl.cc")
    echo "[wasigocvm] WASMNix catalog on EPT (CHPT session, WSL, in-cage hop)"
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

# WASMGocOS and WASMNetStack are in this repo (the canonical trees).
# GocKrnl boots over the Plan 9 session, so the kernel links whole: the
# module set of WASMGocOS's own wasigocvm build (its CMakeLists.txt), with
# goclibc still the host's (no GOCLIBC_IN_KERNEL). The vCPU is the software
# WHv processor (src/vcpu_whp.cc on src/x86.cc) under TurboSpace's
# hv-partition, the pair HeapVM runs.
if [[ ! -f "$ROOT/WASMGocOS/src/session.cc" ]]; then
  echo "error: wasigocvm requires WASMGocOS/ in this repo (src/session.cc)" >&2
  exit 2
fi
if [[ ! -f "$ROOT/WASMNetStack/include/wns/stack.h" ]]; then
  echo "error: wasigocvm requires WASMNetStack/ in this repo (wns/stack.h)" >&2
  exit 2
fi
GOCOS_I=(-I "$ROOT/WASMGocOS/include" -I "$ROOT/WASMNetStack/include" -I "$ROOT/WASMNetStack/src")
GOCOS_D=(-DWASIGO_HAS_WASMGOCOS=1 -DWASMGOCOS_HAS_JITRT=1 -DWASMGOCOS_HAS_VCPU=1 -DWASMGOCOS_HAS_NET=1)
GOCOS_CC=()
for f in catalog unil host_wasi jitrt bbx session sh ctr svc react peinterp wasi files klock stream net x86 vcpu_whp; do
  GOCOS_CC+=("$ROOT/WASMGocOS/src/$f.cc")
done
echo "[wasigocvm] WASMGocOS kernel, Plan 9 session, Sh, Vfs, WHv vCPU"

# WASMWine: kernel32/ntdll PE images for peinterp, when the sibling is there.
WINE_I=()
WINE_CC=()
for d in "${HOME}/WASMWine" "$ROOT/../WASMWine"; do
  if [[ -f "$d/include/wine/dll.h" ]]; then
    WINE_I=(-I "$d/include")
    WINE_CC=("$d/src/dll.cc")
    echo "[wasigocvm] WASMWine PE images"
    break
  fi
done

# WASMTurboSpace: host-calls, the hv-partition, and the engine GocOS runs
# its guests on (wasmturbo_engine as its CMakeLists.txt builds it).
TURBO=""
for d in "${HOME}/WASMTurboSpace" "$ROOT/../WASMTurboSpace"; do
  [[ -f "$d/src/hostcalls/host-calls.cc" ]] && { TURBO="$d"; break; }
done
if [[ -z "$TURBO" ]]; then
  echo "error: wasigocvm requires WASMTurboSpace (host-calls.cc)" >&2
  exit 2
fi
TURBO_I=(-I "$TURBO" -I "$TURBO/include")
TURBO_CC=("$TURBO/src/hostcalls/host-calls.cc" "$TURBO/src/runtime/reactos.cc" "$TURBO/src/exec/executable-memory-posix.cc" "$TURBO/src/hv/hv-partition.cc")
for f in wasm/module-decoder liftoff/liftoff-compiler runtime/trap runtime/exn runtime/imports runtime/context runtime/memory runtime/runtime runtime/intern runtime/call runtime/bulk runtime/lazy runtime/unil runtime/mss runtime/dllwasm runtime/vfs turbofan/graph-builder turbofan/reducer turbofan/scheduler turbofan/codegen turbofan/pipeline interp/interpreter instance; do
  TURBO_CC+=("$TURBO/src/$f.cc")
done
echo "[wasigocvm] WASMTurboSpace host-calls and engine (runtime, Vfs, UNIL, tiers)"

# WASMNetStack: the Plan 9 hop GocKrnl boots over. With WASMReact linked,
# its unity.cc already compiles rpc.cc and locator.cc.
WNS_CC=()
for f in addr classify nat gossip wss chunk demux assoc gls bgp dns ua anycast stack ipify flow proxy sock link proxyd tty status c/system; do
  WNS_CC+=("$ROOT/WASMNetStack/src/$f.cc")
done
[[ -z "$REACT_ROOT" || ${#REACT_CC[@]} -eq 0 ]] && WNS_CC+=("$ROOT/WASMNetStack/src/rpc.cc" "$ROOT/WASMNetStack/src/locator.cc")
echo "[wasigocvm] WASMNetStack (Plan 9 session link)"

CHROME_I=()
CHROME_CC=()
CHROME_D=()
CHROME_ROOT=""
if [[ -f "${HOME}/WASMChrome/include/chrome/host.hpp" ]]; then
  CHROME_ROOT="${HOME}/WASMChrome"
elif [[ -f "$ROOT/WASMChrome/include/chrome/host.hpp" ]]; then
  CHROME_ROOT="$ROOT/WASMChrome"
elif [[ -f "$ROOT/../WASMChrome/include/chrome/host.hpp" ]]; then
  CHROME_ROOT="$ROOT/../WASMChrome"
fi
if [[ -n "$CHROME_ROOT" ]]; then
  CHROME_I=(-I "$CHROME_ROOT/include")
  CHROME_D=(-DWASIGO_HAS_WASMCHROME=1)
  CHROME_CC=("$CHROME_ROOT/src/catalog.cc" "$CHROME_ROOT/src/host.cc" "$CHROME_ROOT/src/webui_catalog.cc")
  echo "[wasigocvm] WASMChrome mojom catalog"
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
    "$SAFE_ROOT/src/sandbox/code-pointer-table.cc"
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
GOCLC_WRAP=()
for sym in fopen fclose fread fwrite fflush fseek ftell fseeko ftello fileno fdopen feof ferror clearerr rewind \
  stat lstat fstat mkdir rmdir unlink remove rename access getcwd chdir \
  opendir readdir closedir pipe dup dup2; do
  GOCLC_WRAP+=("-Wl,--wrap=${sym}")
done
# GocOS containers (ctr.cc): wasmv8_facade over the one quickjs-ng
# WASMv8bindings FetchContents (target qjs, v0.16.2). No second tree.
V8F_CC=()
for f in isolate context exception object template function script primitive; do
  V8F_CC+=("$V8_ROOT/src/v8/$f.cc")
done
QJS_SRC=""
for d in \
  "$V8_ROOT/build-wasi/_deps/quickjs-src" \
  "$V8_ROOT/build/_deps/quickjs-src" \
  "$ROOT/../WASMVoodooCompile/WASMv8bindings/build-wasi/_deps/quickjs-src" \
  "${HOME}/WASMVoodooCompile/WASMv8bindings/build-wasi/_deps/quickjs-src" \
  "$ROOT/../WASMVoodooCompile/WASMv8bindings/build/_deps/quickjs-src" \
  "${HOME}/WASMVoodooCompile/WASMv8bindings/build/_deps/quickjs-src"
do
  if [[ -z "$QJS_SRC" && -f "$d/quickjs.c" ]]; then
    QJS_SRC="$d"
  fi
done
if [[ -z "$QJS_SRC" ]]; then
  echo "error: wasigocvm needs the quickjs WASMv8bindings already fetched (target qjs, v0.16.2)." >&2
  echo "       Configure that repo so wasmv8_facade exists. Do not vendor a second quickjs-ng." >&2
  exit 2
fi
QJS_OUT="$ROOT/build/wasigocvm-quickjs"
QJS_STAMP=""
[[ -f "$QJS_OUT/qjs.src" ]] && QJS_STAMP="$(tr -d '\r' < "$QJS_OUT/qjs.src")"
if [[ "$QJS_STAMP" != "$QJS_SRC" ]]; then
  rm -rf "$QJS_OUT"
fi
mkdir -p "$QJS_OUT"
printf '%s\n' "$QJS_SRC" > "$QJS_OUT/qjs.src"
CC_C="${CLANG/clang++/clang}"
QJS_O=()
for f in quickjs libregexp libunicode dtoa quickjs-libc; do
  if [[ ! -f "$QJS_OUT/$f.o" ]]; then
    "$CC_C" -c -O2 -std=gnu11 -funsigned-char -fvisibility=hidden -mllvm -wasm-enable-sjlj \
      -DQJS_BUILD_LIBC -DQUICKJS_NG_BUILD -D_GNU_SOURCE -D_REENTRANT -D_WASI_EMULATED_GETPID \
      -D_WASI_EMULATED_MMAN -D_WASI_EMULATED_PROCESS_CLOCKS -D_WASI_EMULATED_SIGNAL \
      -isystem "$SYSROOT/include/$TRIPLE" -isystem "$SYSROOT/include" -I "$QJS_SRC" \
      -o "$QJS_OUT/$f.o" "$QJS_SRC/$f.c"
  fi
  QJS_O+=("$QJS_OUT/$f.o")
done
echo "[wasigocvm] quickjs-ng from WASMv8bindings ($QJS_SRC)"

"$CLANG" -O2 -std=c++20 -fuse-ld=lld \
  -fexceptions -frtti \
  -fwasm-exceptions -mllvm -wasm-use-legacy-eh=false \
  -mllvm -wasm-enable-sjlj \
  -nostdinc++ \
  -isystem "$EH_INC" \
  -isystem "$SYSROOT/include/$TRIPLE" \
  -isystem "$SYSROOT/include" \
  -L "$EH_LIB" -lc++ -lc++abi -lunwind \
  -Wl,--export=__indirect_function_table \
  "${GOCLC_WRAP[@]}" \
  "${PTHREAD_FLAGS[@]}" \
  -I "$GO_DIR" -I "$ROOT/src" "${TURBO_I[@]}" "${WIN32_I[@]}" "${NIX_I[@]}" "${DROID_I[@]}" "${GOCOS_I[@]}" "${WINE_I[@]}" "${CHROME_I[@]}" "${PE_I[@]}" "${SAFE_I[@]}" "${V8_I[@]}" "${SSL_I[@]}" "${REACT_I[@]}" "${WEBGPU_I[@]}" -I "$QJS_SRC" \
  -DWASIGO_GOCVM=1 "${CHROME_D[@]}" "${GOCOS_D[@]}" "${WEBGPU_D[@]}" "${SSL_D[@]}" "${REACT_D[@]}" -D_WASI_EMULATED_MMAN -D_WASI_EMULATED_GETPID -D_WASI_EMULATED_SIGNAL -D_WASI_EMULATED_PROCESS_CLOCKS -D_REENTRANT \
  -lwasi-emulated-mman -lwasi-emulated-signal -lwasi-emulated-process-clocks -lsetjmp \
  -o "$OUTWASM" "$GENCPP" "$ROOT/libc/goclibc.c" "$SYSROOT/src/getpid.c" "${TURBO_CC[@]}" "${WIN32_CC[@]}" "${NIX_CC[@]}" "${DROID_CC[@]}" "${GOCOS_CC[@]}" "${WINE_CC[@]}" "${WNS_CC[@]}" "${V8F_CC[@]}" "${QJS_O[@]}" "${CHROME_CC[@]}" "${SAFE_CC[@]}" "${V8_CC[@]}" "${REACT_CC[@]}" "${WEBGPU_CC[@]}" "${SSL_L[@]}"

echo
echo "wasm: $OUTWASM"
echo "run:  $ROOT/wasitime.sh $OUTWASM"
