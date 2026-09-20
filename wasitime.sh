#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

# wasitime — wasigocvm runtime.
# Engine is the ported ~/WASMLoader on the Go++ wazero interpreter
# + WASMSafeSpace. Not a vendor dump of wazero, not w2g.

if [[ $# -lt 1 ]]; then
  echo "wasitime — wasigocvm runtime"
  echo
  echo "Usage:"
  echo "  wasitime inspect <file.wasm>"
  echo "  wasitime run     <file.wasm>"
  echo "  wasitime call    <file.wasm> <export> [i32...]"
  echo "  wasitime link    name=file.wasm ... [--call] module.export [args...]"
  echo "  wasitime example <add|hello|double|plugin> [out.wasm]"
  echo "  wasitime <file.wasm>"
  exit 2
fi

ENGINE_EXE="$ROOT/examples/wasitime/wasitime"
if [[ "$(uname -s)" == MINGW* || "$(uname -s)" == MSYS* || "$(uname -s)" == CYGWIN* ]]; then
  ENGINE_EXE="$ROOT/examples/wasitime/wasitime.exe"
fi

find_wasigoc() {
  for c in \
    "$ROOT/build/Release/wasigoc" \
    "$ROOT/build/wasigoc" \
    "$ROOT/build/wasigoc.exe" \
    "$ROOT/build/Release/wasigoc.exe"
  do
    if [[ -x "$c" ]]; then
      echo "$c"
      return
    fi
  done
}

if [[ ! -x "$ENGINE_EXE" || -n "${WASITIME_REBUILD:-}" ]]; then
  echo "[wasitime] building engine"
  WASIGOC="$(find_wasigoc || true)"
  if [[ -z "${WASIGOC:-}" ]]; then
    echo "error: wasigoc not found — build it first" >&2
    exit 1
  fi
  GO_ABS="$ROOT/examples/wasitime/main.go"
  GENCPP="$ROOT/examples/wasitime/main_gen.cpp"
  "$WASIGOC" "$GO_ABS" -o "$GENCPP" --out-dir "$ROOT/examples/wasitime"
  CXX="${CXX:-clang++}"
  "$CXX" -std=c++20 -O2 -I "$ROOT/examples/wasitime" -I "$ROOT/src" "$GENCPP" -o "$ENGINE_EXE" -lpthread
  if [[ -n "${WASITIME_WASM:-}" ]]; then
    export WASIGO_TOOLCHAIN="${WASIGO_TOOLCHAIN:-$ROOT/toolchain}"
    "$ROOT/wasigocvm.sh" examples/wasitime/main.go -o examples/wasitime/wasitime.wasm
  fi
fi

exec "$ENGINE_EXE" "$@"
