#!/usr/bin/env bash
# bootstrap.sh — build a wasigocvm sysroot into toolchain/
#
# Usage (from repo root, Linux / macOS / WSL):
#   ./toolchain/bootstrap.sh
#   ./toolchain/bootstrap.sh --jobs 8
#   WASI_SDK_REF=wasi-sdk-33 ./toolchain/bootstrap.sh
#
# Installs under:
#   toolchain/          (CMAKE_INSTALL_PREFIX)
#   toolchain/sysroot/  (wasi-sysroot with eh libc++)
#   toolchain/bin/      (clang wrappers)
#
# This is a long build (LLVM + wasi-libc + libc++). Prefer CI or overnight WSL.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TC="$ROOT/toolchain"
# Default source/build trees live under $HOME when the repo is on a slow
# mount (WSL /mnt/c). Install prefix stays in-repo so Windows drivers see it.
if [[ -z "${WASIGO_WASI_SDK_SRC:-}" ]]; then
  case "$ROOT" in
    /mnt/*) SRC="$HOME/wasigocvm-wasi-sdk-src" ;;
    *)      SRC="$TC/wasi-sdk-src" ;;
  esac
else
  SRC="$WASIGO_WASI_SDK_SRC"
fi
if [[ -z "${WASIGO_WASI_SDK_BUILD:-}" ]]; then
  case "$ROOT" in
    /mnt/*) BUILD="$HOME/wasigocvm-wasi-sdk-build" ;;
    *)      BUILD="$TC/wasi-sdk-build" ;;
  esac
else
  BUILD="$WASIGO_WASI_SDK_BUILD"
fi
PREFIX="${WASIGO_TOOLCHAIN:-$TC}"
REF="${WASI_SDK_REF:-main}"
JOBS="${WASIGO_BOOTSTRAP_JOBS:-$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --jobs|-j) JOBS="$2"; shift 2 ;;
    --ref) REF="$2"; shift 2 ;;
    --prefix) PREFIX="$2"; shift 2 ;;
    -h|--help)
      sed -n '2,16p' "$0"
      exit 0
      ;;
    *) echo "unknown arg: $1" >&2; exit 2 ;;
  esac
done

mkdir -p "$TC"
echo "[wasigocvm] bootstrap → prefix=$PREFIX jobs=$JOBS ref=$REF"

if [[ ! -d "$SRC/.git" ]]; then
  echo "[wasigocvm] cloning WebAssembly/wasi-sdk ($REF)…"
  git clone --recursive --branch "$REF" \
    https://github.com/WebAssembly/wasi-sdk.git "$SRC" \
    || git clone --recursive https://github.com/WebAssembly/wasi-sdk.git "$SRC"
fi

# Keep submodules current if we already had a checkout.
git -C "$SRC" submodule update --init --recursive

echo "[wasigocvm] configuring (WASI_SDK_EXCEPTIONS=ON)…"
cmake -S "$SRC" -B "$BUILD" \
  -DCMAKE_INSTALL_PREFIX="$PREFIX" \
  -DWASI_SDK_EXCEPTIONS=ON \
  ${CMAKE_GENERATOR:+-G "$CMAKE_GENERATOR"}

echo "[wasigocvm] building + installing (this takes a long time)…"
cmake --build "$BUILD" --target install -j "$JOBS"

# wasi-sdk installs sysroot at share/wasi-sysroot; wasigocvm.bat prefers
# toolchain/sysroot. Symlink or copy if needed.
if [[ -d "$PREFIX/share/wasi-sysroot" && ! -e "$PREFIX/sysroot" ]]; then
  ln -sfn "$PREFIX/share/wasi-sysroot" "$PREFIX/sysroot" \
    || { mkdir -p "$PREFIX/sysroot" && cp -a "$PREFIX/share/wasi-sysroot/." "$PREFIX/sysroot/"; }
fi

# Stamp identity so drivers know this is a wasigocvm install.
mkdir -p "$PREFIX"
cat > "$PREFIX/wasigocvm-toolchain.json" <<EOF
{
  "name": "wasigocvm",
  "exceptions": true,
  "threads": false,
  "borrowed_triple": "wasm32-wasip2",
  "product_define": "WASIGO_GOCVM",
  "built_at": "$(date -u +%Y-%m-%dT%H:%M:%SZ)",
  "wasi_sdk_ref": "$REF"
}
EOF

# Convenience wrapper name (forwards to wasip2 clang until we own a triple).
BIN="$PREFIX/bin"
if [[ -x "$BIN/wasm32-wasip2-clang++" ]]; then
  cat > "$BIN/wasm32-wasigocvm-clang++" <<'WRAP'
#!/usr/bin/env bash
# Temporary: wasigocvm identity wrapper → stock wasip2 clang++ from this prefix.
DIR="$(cd "$(dirname "$0")" && pwd)"
exec "$DIR/wasm32-wasip2-clang++" "$@"
WRAP
  chmod +x "$BIN/wasm32-wasigocvm-clang++"
fi

echo
echo "[wasigocvm] done."
echo "  export WASIGO_TOOLCHAIN=$PREFIX"
echo "  wasigocvm.bat / wasigocvm.sh will prefer this sysroot."
echo "  threads: still false — next patch folds shared-everything-threads."
