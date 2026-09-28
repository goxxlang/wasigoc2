@echo off
setlocal
cd /d "%~dp0"
set "TC=%~dp0..\..\toolchain"
where gcc >nul 2>&1
if errorlevel 1 (
  echo error: gcc not found — native payload.dll is a PE image, not wasm
  exit /b 1
)
gcc -shared -o "%~dp0payload.dll" "%~dp0payload.c"
if errorlevel 1 exit /b 1
if not exist "%TC%\bin\wasm32-wasip2-clang.exe" (
  echo error: toolchain clang missing
  exit /b 1
)
"%TC%\bin\wasm32-wasip2-clang.exe" -O2 -fPIC -shared -mexec-model=reactor ^
  -o "%~dp0loadpe.wasm" "%~dp0loadpe.c"
if errorlevel 1 exit /b 1
python "%~dp0stamp_native.py" "%~dp0loadpe.wasm"
if errorlevel 1 exit /b 1
echo wasm: %~dp0loadpe.wasm
echo  pe:  %~dp0payload.dll
exit /b 0
