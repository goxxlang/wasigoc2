# Build OpenSSL 3.6.3 for wasigocvm wasm via WSL (same tree WASMLime uses).
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$wsl = Get-Command wsl -ErrorAction SilentlyContinue
if (-not $wsl) {
  Write-Error "wsl required for toolchain/build_openssl_wasm.sh (perl+make)"
}
$unixRoot = (wsl -e wslpath -a $Root).Trim()
wsl -e bash -lc "cd '$unixRoot' && chmod +x toolchain/build_openssl_wasm.sh && toolchain/build_openssl_wasm.sh $($args -join ' ')"
