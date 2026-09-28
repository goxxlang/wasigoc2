@echo off
setlocal
rem DLL WASM module: toolchain clang -shared -fPIC, reactor (no _start).
rem   build.bat
rem   build.bat other.c -o other.wasm
cd /d "%~dp0"
set "TC=%~dp0..\..\toolchain"
set "SRC=%~dp0add.c"
set "OUT=%~dp0add.wasm"
if not "%~1"=="" set "SRC=%~1"
if /I "%~2"=="-o" if not "%~3"=="" set "OUT=%~3"
if not exist "%TC%\bin\wasm32-wasip2-clang.exe" (
  echo error: toolchain clang missing: %TC%\bin\wasm32-wasip2-clang.exe
  exit /b 1
)
"%TC%\bin\wasm32-wasip2-clang.exe" -O2 -fPIC -shared -mexec-model=reactor -o "%OUT%" "%SRC%"
if errorlevel 1 exit /b 1
echo wasm: %OUT%
exit /b 0
