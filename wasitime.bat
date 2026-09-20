@echo off
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"

rem wasitime — wasigocvm runtime (wasmtime-shaped).
rem
rem   wasitime inspect file.wasm
rem   wasitime call    file.wasm add 2 3
rem   wasitime run     file.wasm
rem   wasitime file.wasm
rem   wasitime example add|hello [out.wasm]
rem
rem Engine is the ported ~/WASMLoader (wasmbin + loader) on the Go++
rem wazero interpreter + WASMSafeSpace. Not a vendor dump of wazero,
rem not w2g/Wasmtime.

if "%~1"=="" (
  echo wasitime — wasigocvm runtime
  echo.
  echo Usage:
  echo   wasitime inspect ^<file.wasm^>
  echo   wasitime run     ^<file.wasm^>
  echo   wasitime call    ^<file.wasm^> ^<export^> [i32...]
  echo   wasitime link    name=file.wasm ... [--call] module.export [args...]
  echo   wasitime example ^<add^|hello^|double^|plugin^> [out.wasm]
  echo   wasitime ^<file.wasm^>
  exit /b 2
)

set "ENGINE_EXE=%cd%\examples\wasitime\wasitime.exe"
set "ENGINE_WASM=%cd%\examples\wasitime\wasitime.wasm"
if not exist "%ENGINE_EXE%" set "WASITIME_REBUILD=1"
if defined WASITIME_REBUILD (
  echo [wasitime] building engine
  call goclang++.bat examples\wasitime\main.go -o examples\wasitime\wasitime.exe --no-shim-sandbox -lpthread
  if errorlevel 1 exit /b 1
  if defined WASITIME_WASM (
    set "WASIGO_TOOLCHAIN=%cd%\toolchain"
    call wasigocvm.bat examples\wasitime\main.go -o examples\wasitime\wasitime.wasm
    if errorlevel 1 exit /b 1
  )
)
if not exist "%ENGINE_EXE%" (
  echo error: wasitime.exe not built
  exit /b 1
)
"%ENGINE_EXE%" %*
exit /b %ERRORLEVEL%
