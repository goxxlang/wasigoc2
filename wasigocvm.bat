@echo off
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"
set "WASIGO_ROOT=%CD%"

rem wasigocvm - Go++ -> C++ -> wasm using OUR toolchain contract.
rem Product define: -DWASIGO_GOCVM=1. compile.bat is the default driver
rem (this file is the same machine). Stock wasip1 leftover: legacy.bat.
rem
rem   compile.bat examples\httppkg\main.go
rem   wasigocvm.bat hello.go -o out.wasm

set "CONFIG=Release"
set "GOFILE="
set "OUTWASM="
set "WASIGOC_EXTRA="

:parse
if "%~1"=="" goto :parsed
if /I "%~1"=="-h" goto :usage
if /I "%~1"=="--help" goto :usage
if /I "%~1"=="/?" goto :usage
if /I "%~1"=="-o" (
  if "%~2"=="" (echo error: -o requires a path & exit /b 2)
  set "OUTWASM=%~2"
  shift & shift & goto :parse
)
if /I "%~1"=="Debug" (set "CONFIG=Debug" & shift & goto :parse)
if /I "%~1"=="Release" (set "CONFIG=Release" & shift & goto :parse)
if /I "%~x1"==".go" (
  if defined GOFILE (echo error: extra .go file & exit /b 2)
  set "GOFILE=%~1" & shift & goto :parse
)
set "WASIGOC_EXTRA=!WASIGOC_EXTRA! %1"
shift
goto :parse

:parsed
call :find_wasigoc
if not defined WASIGOC_EXE (
  echo building wasigoc...
  call "%~dp0legacy.bat" %CONFIG%
  if errorlevel 1 exit /b 1
  call :find_wasigoc
)
if not defined WASIGOC_EXE (echo error: wasigoc.exe not found & exit /b 1)
if not defined GOFILE (
  echo wasigocvm: full libc++ / eh / rtti / WASIGO_GOCVM ^(docs\wasigocvm.md^)
  echo usage: wasigocvm.bat ^<file.go^> [-o out.wasm]
  exit /b 0
)
if not exist "%GOFILE%" (echo error: not found: %GOFILE% & exit /b 1)

for %%I in ("%GOFILE%") do (
  set "GO_ABS=%%~fI"
  set "GO_DIR=%%~dpI"
  set "GO_BASE=%%~nI"
)
if "%GO_DIR:~-1%"=="\" set "GO_DIR=%GO_DIR:~0,-1%"
set "GENCPP=%GO_DIR%\%GO_BASE%_gen.cpp"
if not defined OUTWASM set "OUTWASM=%GO_DIR%\%GO_BASE%.wasm"

rem WASMReact (ReactOS FreeLoader + ntoskrnl on GocOS): optional, like
rem WASMChrome. Found before wasigoc runs so `import "reactos"` resolves
rem to its Go++ facade. Its IPT comes from the WASMSafeSpace tree that
rem has one (go++'s copy does not).
set "WASIGO_REACT="
if exist "%USERPROFILE%\WASMReact\include\reactos\dispatch.h" set "WASIGO_REACT=%USERPROFILE%\WASMReact"
if not defined WASIGO_REACT if exist "%WASIGO_ROOT%\..\WASMReact\include\reactos\dispatch.h" for %%I in ("%WASIGO_ROOT%\..\WASMReact") do set "WASIGO_REACT=%%~fI"
set "WASIGO_REACT_IPT="
if exist "%USERPROFILE%\WASMVoodooCompile\WASMSafeSpace\src\sandbox\indirect-pointer-table.cc" set "WASIGO_REACT_IPT=%USERPROFILE%\WASMVoodooCompile\WASMSafeSpace"
if defined WASIGO_REACT if not defined WASIGO_REACT_IPT (
  echo warning: WASMReact needs WASMSafeSpace with indirect-pointer-table.cc ^(~/WASMVoodooCompile^); not linking it
  set "WASIGO_REACT="
)
set "WASIGO_WPR="
if exist "%USERPROFILE%\WASMProcess\include\wpr\processes.h" set "WASIGO_WPR=%USERPROFILE%\WASMProcess"
if not defined WASIGO_WPR if exist "%WASIGO_ROOT%\..\WASMProcess\include\wpr\processes.h" for %%I in ("%WASIGO_ROOT%\..\WASMProcess") do set "WASIGO_WPR=%%~fI"
if defined WASIGO_REACT if not defined WASIGO_WPR (
  echo warning: WASMReact smss needs WASMProcess; not linking it
  set "WASIGO_REACT="
)
if defined WASIGO_REACT set WASIGOC_EXTRA=!WASIGOC_EXTRA! --import-dir="!WASIGO_REACT!\go"

rem WASMWebGPU links only into a program that imports "webgpu": wasigoc
rem writes stdlib_webgpu_gen.hpp then. A stale one from an earlier build
rem must not count, so it goes first.
if exist "%GO_DIR%\stdlib_webgpu_gen.hpp" del /q "%GO_DIR%\stdlib_webgpu_gen.hpp"

echo [wasigoc] %GO_ABS%
"%WASIGOC_EXE%" "%GO_ABS%" -o "%GENCPP%" --out-dir "%GO_DIR%" %WASIGOC_EXTRA%
if errorlevel 1 exit /b 1

set "WASIGO_WEBGPU_I="
set "WASIGO_WEBGPU_D="
set "WASIGO_WEBGPU_CC="
if exist "%GO_DIR%\stdlib_webgpu_gen.hpp" (
  if not exist "%WASIGO_ROOT%\WASMWebGPU\src\catalog.cc" (
    echo error: this program imports webgpu but WASMWebGPU\ is missing
    exit /b 1
  )
  set WASIGO_WEBGPU_I=-I "%WASIGO_ROOT%\WASMWebGPU\include"
  set WASIGO_WEBGPU_D=-DWASIGO_HAS_WEBGPU=1
  set WASIGO_WEBGPU_CC="%WASIGO_ROOT%\WASMWebGPU\src\catalog.cc" "%WASIGO_ROOT%\WASMWebGPU\src\remote.cc"
  echo             -I "%WASIGO_ROOT%\WASMWebGPU\include" ^(webgpu catalog: import webgpu.batch^)
)

call :find_toolchain
if not defined WASIGO_CLANG (
  echo error: no wasigocvm clang - set WASIGO_TOOLCHAIN or run toolchain\bootstrap.ps1
  echo          ^(fallback: %%USERPROFILE%%\wasi-sdk with eh/^)
  exit /b 1
)

call :pick_triple
if not defined WASIGO_TRIPLE (
  echo error: full libc++ ^(eh^) missing under %WASIGO_SYSROOT%\include
  echo        run toolchain\bootstrap.ps1 or use a wasi-sdk with eh/
  exit /b 1
)

set "EH_INC=%WASIGO_SYSROOT%\include\%WASIGO_TRIPLE%\eh\c++\v1"
set "EH_LIB=%WASIGO_SYSROOT%\lib\%WASIGO_TRIPLE%\eh"

rem Threads: stamp threads:true + libpthread (prefer this triple so sockets stay).
set "WASIGO_THREADS="
set "WASIGO_PTHREAD_FLAGS="
set "STAMP=%~dp0toolchain\wasigocvm-toolchain.json"
if defined WASIGO_SDK_ROOT if exist "%WASIGO_SDK_ROOT%\wasigocvm-toolchain.json" set "STAMP=%WASIGO_SDK_ROOT%\wasigocvm-toolchain.json"
if exist "%STAMP%" (
  findstr /R /C:"threads.:.*true" "%STAMP%" >nul 2>&1 && set "WASIGO_THREADS=1"
)
if defined WASIGO_THREADS (
  set "PTHREAD_LIBDIR="
  if exist "%WASIGO_SYSROOT%\lib\%WASIGO_TRIPLE%\libpthread.a" set "PTHREAD_LIBDIR=%WASIGO_SYSROOT%\lib\%WASIGO_TRIPLE%"
  if not defined PTHREAD_LIBDIR if exist "%USERPROFILE%\wasi-sdk\share\wasi-sysroot\lib\%WASIGO_TRIPLE%\libpthread.a" set "PTHREAD_LIBDIR=%USERPROFILE%\wasi-sdk\share\wasi-sysroot\lib\%WASIGO_TRIPLE%"
  if not defined PTHREAD_LIBDIR if exist "%WASIGO_SYSROOT%\lib\wasm32-wasip1-threads\libpthread.a" set "PTHREAD_LIBDIR=%WASIGO_SYSROOT%\lib\wasm32-wasip1-threads"
  if defined PTHREAD_LIBDIR (
    rem WASMSafeSpace: wasm32 linear memory already is the process boundary.
    rem Do not pass --shared-memory (eh libc++ was not built with atomics;
    rem TLS errno disagrees with wasip2 libc). Child working set lives in
    rem ~/WASMSafeSpace Sandbox (heap cage). -D_REENTRANT without -pthread
    rem so errno stays non-TLS. std::thread still links -lpthread.
    set WASIGO_PTHREAD_FLAGS=-D_REENTRANT -L "%PTHREAD_LIBDIR%" -lpthread
  ) else (
    echo warning: stamp threads:true but libpthread.a missing - building without -pthread
    set "WASIGO_THREADS="
    set "WASIGO_PTHREAD_FLAGS="
  )
)


echo [wasigocvm] %WASIGO_CLANG%
echo             triple=%WASIGO_TRIPLE%  full libc++ + rtti + standard wasm EH
echo             setjmp/longjmp on wasm EH ^(libsetjmp __c_longjmp^)
if defined WASIGO_THREADS echo             threads=on  (std::thread fork, WASMSafeSpace cage, no wasm shared-memory)
rem Resolve <go++>/src even if %~dp0 is toolchain\ (hardlink / odd invoke)
set "WASIGO_SRC_INC="
if exist "%~dp0src\wasigocvm_config.hpp" set "WASIGO_SRC_INC=%~dp0src"
if not defined WASIGO_SRC_INC if exist "%~dp0..\src\wasigocvm_config.hpp" for %%I in ("%~dp0..\src") do set "WASIGO_SRC_INC=%%~fI"
if not defined WASIGO_SRC_INC set "WASIGO_SRC_INC=C:\Users\grego\go++\src"
echo             -I "%GO_DIR%" -I "%WASIGO_SRC_INC%"
set "WASIGO_PE="
if exist "%WASIGO_ROOT%\WASMPELoader\include\wasmpe\loader.hpp" set "WASIGO_PE=%WASIGO_ROOT%\WASMPELoader"
if not defined WASIGO_PE if exist "%USERPROFILE%\WASMPELoader\include\wasmpe\loader.hpp" set "WASIGO_PE=%USERPROFILE%\WASMPELoader"
if not defined WASIGO_PE if exist "%WASIGO_ROOT%\..\WASMPELoader\include\wasmpe\loader.hpp" for %%I in ("%WASIGO_ROOT%\..\WASMPELoader") do set "WASIGO_PE=%%~fI"
set "WASIGO_PE_I="
if defined WASIGO_PE (
  set WASIGO_PE_I=-I "!WASIGO_PE!\include"
  echo             -I "!WASIGO_PE!\include" ^(WASMPELoader^)
)
if exist "%WASIGO_ROOT%\WASMWin32\include\win32\wasi_host.hpp" set "WASIGO_WIN32=%WASIGO_ROOT%\WASMWin32"
if not defined WASIGO_WIN32 (
  echo error: wasigocvm requires WASMWin32\ in this repo ^(win32/wasi_host.hpp^)
  exit /b 2
)
set "WASIGO_WIN32_I="
set "WASIGO_WIN32_CC="
if defined WASIGO_WIN32 (
  set WASIGO_WIN32_I=-I "!WASIGO_WIN32!\include"
  echo             -I "!WASIGO_WIN32!\include" ^(WASMWin32 libc host^)
  if exist "!WASIGO_WIN32!\src\catalog.cc" (
    set WASIGO_WIN32_CC="!WASIGO_WIN32!\src\catalog.cc"
    echo             WASMWin32 catalog on EPT ^(TPT process/thread, CHPT session^)
  )
  if exist "!WASIGO_WIN32!\src\host_wasi.cc" (
    set WASIGO_WIN32_CC=!WASIGO_WIN32_CC! "!WASIGO_WIN32!\src\host_wasi.cc"
    echo             WASMWin32 wasmwin32_call ^(CreateProcessW^)
  )
)
if exist "%WASIGO_ROOT%\WASMNix\include\nix\wsl.hpp" set "WASIGO_NIX=%WASIGO_ROOT%\WASMNix"
if not defined WASIGO_NIX (
  echo error: wasigocvm requires WASMNix\ in this repo ^(nix/wsl.hpp^)
  exit /b 2
)
set "WASIGO_NIX_I="
set "WASIGO_NIX_CC="
if defined WASIGO_NIX (
  set WASIGO_NIX_I=-I "!WASIGO_NIX!\include"
  echo             -I "!WASIGO_NIX!\include" ^(WASMNix WSL^)
  if exist "!WASIGO_NIX!\src\catalog.cc" (
    set WASIGO_NIX_CC="!WASIGO_NIX!\src\catalog.cc" "!WASIGO_NIX!\src\call.cc" "!WASIGO_NIX!\src\cage_wsl.cc"
    echo             WASMNix catalog on EPT ^(CHPT session, WSL, in-cage hop^)
  )
)
if exist "%WASIGO_ROOT%\WASMDroid\include\droid\phonelink.hpp" set "WASIGO_DROID=%WASIGO_ROOT%\WASMDroid"
if not defined WASIGO_DROID (
  echo error: wasigocvm requires WASMDroid\ in this repo ^(droid/phonelink.hpp^)
  exit /b 2
)
set "WASIGO_DROID_I="
set "WASIGO_DROID_CC="
if defined WASIGO_DROID (
  set WASIGO_DROID_I=-I "!WASIGO_DROID!\include"
  echo             -I "!WASIGO_DROID!\include" ^(WASMDroid Phone Link^)
  if exist "!WASIGO_DROID!\src\catalog.cc" (
    set WASIGO_DROID_CC="!WASIGO_DROID!\src\catalog.cc" "!WASIGO_DROID!\src\call.cc"
    echo             WASMDroid catalog on EPT ^(CHPT session, Phone Link^)
  )
)
rem WASMGocOS and WASMNetStack are in this repo (the canonical trees).
rem GocKrnl boots over the Plan 9 session, so the kernel links whole: the
rem module set of WASMGocOS's own wasigocvm build (its CMakeLists.txt),
rem with goclibc still the host's (no GOCLIBC_IN_KERNEL). The vCPU is the
rem software WHv processor (src\vcpu_whp.cc on the x86 engine, src\x86.cc)
rem under TurboSpace's hv-partition, the pair HeapVM runs.
set "WASIGO_GOCOS="
set "WASIGO_WNS="
if exist "%WASIGO_ROOT%\WASMGocOS\src\session.cc" set "WASIGO_GOCOS=%WASIGO_ROOT%\WASMGocOS"
if not defined WASIGO_GOCOS (
  echo error: wasigocvm requires WASMGocOS\ in this repo ^(src/session.cc^)
  exit /b 2
)
if exist "%WASIGO_ROOT%\WASMNetStack\include\wns\stack.h" set "WASIGO_WNS=%WASIGO_ROOT%\WASMNetStack"
if not defined WASIGO_WNS (
  echo error: wasigocvm requires WASMNetStack\ in this repo ^(wns/stack.h^)
  exit /b 2
)
set WASIGO_GOCOS_I=-I "!WASIGO_GOCOS!\include" -I "!WASIGO_WNS!\include"
set WASIGO_GOCOS_D=-DWASIGO_HAS_WASMGOCOS=1 -DWASMGOCOS_HAS_JITRT=1 -DWASMGOCOS_HAS_VCPU=1 -DWASMGOCOS_HAS_NET=1
set WASIGO_GOCOS_CC=
for %%F in (catalog unil host_wasi jitrt bbx session sh ctr svc react peinterp wasi files klock stream net x86 vcpu_whp) do set WASIGO_GOCOS_CC=!WASIGO_GOCOS_CC! "!WASIGO_GOCOS!\src\%%F.cc"
echo             -I "!WASIGO_GOCOS!\include" ^(WASMGocOS kernel, Plan 9 session, Sh, Vfs, WHv vCPU^)
rem WASMWine: kernel32/ntdll PE images for peinterp, when the sibling is there.
set "WASIGO_WINE="
set "WASIGO_WINE_I="
set "WASIGO_WINE_CC="
if exist "%USERPROFILE%\WASMWine\include\wine\dll.h" set "WASIGO_WINE=%USERPROFILE%\WASMWine"
if not defined WASIGO_WINE if exist "%WASIGO_ROOT%\..\WASMWine\include\wine\dll.h" for %%I in ("%WASIGO_ROOT%\..\WASMWine") do set "WASIGO_WINE=%%~fI"
if defined WASIGO_WINE (
  set WASIGO_WINE_I=-I "!WASIGO_WINE!\include"
  set WASIGO_WINE_CC="!WASIGO_WINE!\src\dll.cc"
  echo             -I "!WASIGO_WINE!\include" ^(WASMWine PE images^)
)
set "WASIGO_CHROME="
if exist "%USERPROFILE%\WASMChrome\include\chrome\host.hpp" set "WASIGO_CHROME=%USERPROFILE%\WASMChrome"
if not defined WASIGO_CHROME if exist "%WASIGO_ROOT%\WASMChrome\include\chrome\host.hpp" set "WASIGO_CHROME=%WASIGO_ROOT%\WASMChrome"
if not defined WASIGO_CHROME if exist "%WASIGO_ROOT%\..\WASMChrome\include\chrome\host.hpp" for %%I in ("%WASIGO_ROOT%\..\WASMChrome") do set "WASIGO_CHROME=%%~fI"
set "WASIGO_CHROME_I="
set "WASIGO_CHROME_CC="
set "WASIGO_CHROME_D="
if defined WASIGO_CHROME (
  set WASIGO_CHROME_I=-I "!WASIGO_CHROME!\include"
  set WASIGO_CHROME_D=-DWASIGO_HAS_WASMCHROME=1
  set WASIGO_CHROME_CC="!WASIGO_CHROME!\src\catalog.cc" "!WASIGO_CHROME!\src\host.cc" "!WASIGO_CHROME!\src\webui_catalog.cc"
  echo             -I "!WASIGO_CHROME!\include" ^(WASMChrome mojom catalog^)
)
set "WASIGO_SAFE="
if exist "%WASIGO_ROOT%\WASMSafeSpace\src\sandbox\sandbox.cc" set "WASIGO_SAFE=%WASIGO_ROOT%\WASMSafeSpace"
if not defined WASIGO_SAFE (
  echo error: wasigocvm requires WASMSafeSpace\ in this repo ^(sandbox.cc^)
  exit /b 2
)
set "WASIGO_SAFE_I="
set "WASIGO_SAFE_CC="
if defined WASIGO_SAFE (
  set WASIGO_SAFE_I=-I "!WASIGO_SAFE!\include" -I "!WASIGO_SAFE!"
  set WASIGO_SAFE_CC="!WASIGO_SAFE!\src\sandbox\sandbox.cc" "!WASIGO_SAFE!\src\sandbox\external-pointer-table.cc" "!WASIGO_SAFE!\src\sandbox\trusted-pointer-table.cc" "!WASIGO_SAFE!\src\sandbox\code-pointer-table.cc"
  echo             WASMSafeSpace EPT/TPT ^(exec second address space^)
)
set "WASIGO_V8="
if exist "%WASIGO_ROOT%\WASMv8bindings\include\cppgc\heap.h" set "WASIGO_V8=%WASIGO_ROOT%\WASMv8bindings"
if not defined WASIGO_V8 (
  echo error: wasigocvm requires WASMv8bindings\ in this repo ^(cppgc/heap.h^)
  exit /b 2
)
set "WASIGO_V8_I="
set "WASIGO_V8_CC="
if defined WASIGO_V8 (
  set WASIGO_V8_I=-I "!WASIGO_V8!\include" -I "!WASIGO_V8!"
  set WASIGO_V8_CC="!WASIGO_V8!\src\heap\internal\cppgc-logging.cc" "!WASIGO_V8!\src\heap\internal\free-list.cc" "!WASIGO_V8!\src\heap\internal\gc-info.cc" "!WASIGO_V8!\src\heap\internal\gc-info-table.cc" "!WASIGO_V8!\src\heap\internal\heap.cc" "!WASIGO_V8!\src\heap\internal\platform.cc" "!WASIGO_V8!\src\sandbox\cppheap-pointer-table.cc"
  echo             WASMv8bindings CHPT ^(exec second address space^)
)
set "WASIGO_SSL_I="
set "WASIGO_SSL_L="
set "WASIGO_SSL_D="
set "WASIGO_SSL_INC="
set "WASIGO_SSL_LIB="
if exist "%WASIGO_ROOT%\toolchain\openssl-wasm\lib\libssl.a" (
  if exist "%WASIGO_ROOT%\toolchain\openssl-wasm\include\openssl\ssl.h" (
    set "WASIGO_SSL_INC=%WASIGO_ROOT%\toolchain\openssl-wasm\include"
    set "WASIGO_SSL_LIB=%WASIGO_ROOT%\toolchain\openssl-wasm\lib"
  )
)
if not defined WASIGO_SSL_INC if exist "%WASIGO_SYSROOT%\include\openssl\ssl.h" (
  if exist "%WASIGO_SYSROOT%\lib\%WASIGO_TRIPLE%\libssl.a" (
    set "WASIGO_SSL_INC=%WASIGO_SYSROOT%\include"
    set "WASIGO_SSL_LIB=%WASIGO_SYSROOT%\lib\%WASIGO_TRIPLE%"
  )
)
if defined WASIGO_SSL_INC (
  set WASIGO_SSL_I=-I "!WASIGO_SSL_INC!"
  set WASIGO_SSL_L="!WASIGO_SSL_LIB!\libssl.a" "!WASIGO_SSL_LIB!\libcrypto.a"
  set WASIGO_SSL_D=-DWASIGO_HAS_OPENSSL=1
  echo             OpenSSL wasm ^(WASMLime TlsTransport / memory BIO^)
)
set "WASIGO_REACT_I="
set "WASIGO_REACT_CC="
set "WASIGO_REACT_D="
if defined WASIGO_REACT (
  set WASIGO_REACT_I=-I "!WASIGO_REACT!\include" -I "!WASIGO_REACT_IPT!" -I "!WASIGO_WPR!\include" -I "!WASIGO_WPR!\src" -I "!USERPROFILE!\WASMChrome\include"
  set WASIGO_REACT_CC="!WASIGO_REACT!\src\unity.cc" "!WASIGO_REACT_IPT!\src\sandbox\indirect-pointer-table.cc" "!WASIGO_REACT!\src\ps\drvfs.cc"
  set WASIGO_REACT_D=-DWASIGO_HAS_WASMREACT=1
  echo             -I "!WASIGO_REACT!\include" ^(WASMReact ReactOS kernel on EPT/TPT/CHPT/IPT^)
)
set "WASIGO_TURBO="
if exist "%USERPROFILE%\WASMTurboSpace\src\hostcalls\host-calls.cc" set "WASIGO_TURBO=%USERPROFILE%\WASMTurboSpace"
if not defined WASIGO_TURBO if exist "%WASIGO_ROOT%\..\WASMTurboSpace\src\hostcalls\host-calls.cc" for %%I in ("%WASIGO_ROOT%\..\WASMTurboSpace") do set "WASIGO_TURBO=%%~fI"
if not defined WASIGO_TURBO (
  echo error: wasigocvm requires WASMTurboSpace ^(host-calls.cc^)
  exit /b 2
)
set "WASIGO_TURBO_I=-I "!WASIGO_TURBO!" -I "!WASIGO_TURBO!\include""
set "WASIGO_TURBO_CC="!WASIGO_TURBO!\src\hostcalls\host-calls.cc" "!WASIGO_TURBO!\src\runtime\reactos.cc" "!WASIGO_TURBO!\src\exec\executable-memory-posix.cc" "!WASIGO_TURBO!\src\hv\hv-partition.cc""
rem The engine GocOS runs its guests on (WASMTurboSpace wasmturbo_engine,
rem as its CMakeLists.txt builds it for wasigocvm).
for %%F in (wasm\module-decoder liftoff\liftoff-compiler runtime\trap runtime\exn runtime\imports runtime\context runtime\memory runtime\runtime runtime\intern runtime\call runtime\bulk runtime\lazy runtime\unil runtime\mss runtime\dllwasm runtime\vfs turbofan\graph-builder turbofan\reducer turbofan\scheduler turbofan\codegen turbofan\pipeline interp\interpreter instance) do set WASIGO_TURBO_CC=!WASIGO_TURBO_CC! "!WASIGO_TURBO!\src\%%F.cc"
echo             WASMTurboSpace host-calls and engine ^(runtime, Vfs, UNIL, tiers^)
rem WASMNetStack: the Plan 9 hop GocKrnl boots over (WASMGocOS net.cc). With
rem WASMReact linked, its unity.cc already compiles rpc.cc and locator.cc.
set WASIGO_WNS_CC=
for %%F in (addr classify nat gossip wss chunk demux assoc gls bgp dns ua anycast stack ipify flow proxy sock link proxyd tty status c\system) do set WASIGO_WNS_CC=!WASIGO_WNS_CC! "!WASIGO_WNS!\src\%%F.cc"
if not defined WASIGO_REACT set WASIGO_WNS_CC=!WASIGO_WNS_CC! "!WASIGO_WNS!\src\rpc.cc" "!WASIGO_WNS!\src\locator.cc"
set WASIGO_WNS_I=-I "!WASIGO_WNS!\src"
echo             WASMNetStack ^(Plan 9 session link^)
rem GocOS containers (ctr.cc): wasmv8_facade over the one quickjs-ng
rem WASMv8bindings FetchContents (target qjs, v0.16.2). No second tree.
set WASIGO_V8F_CC=
for %%F in (isolate context exception object template function script primitive) do set WASIGO_V8F_CC=!WASIGO_V8F_CC! "!WASIGO_V8!\src\v8\%%F.cc"
set "QJS_SRC="
for %%D in (
  "!WASIGO_V8!\build-wasi\_deps\quickjs-src"
  "!WASIGO_V8!\build\_deps\quickjs-src"
  "%WASIGO_ROOT%\..\WASMVoodooCompile\WASMv8bindings\build-wasi\_deps\quickjs-src"
  "%USERPROFILE%\WASMVoodooCompile\WASMv8bindings\build-wasi\_deps\quickjs-src"
  "%WASIGO_ROOT%\..\WASMVoodooCompile\WASMv8bindings\build\_deps\quickjs-src"
  "%USERPROFILE%\WASMVoodooCompile\WASMv8bindings\build\_deps\quickjs-src"
) do (
  if not defined QJS_SRC if exist "%%~D\quickjs.c" set "QJS_SRC=%%~D"
)
if not defined QJS_SRC (
  echo error: wasigocvm needs the quickjs WASMv8bindings already fetched ^(target qjs, v0.16.2^).
  echo        Configure that repo so wasmv8_facade exists. Do not vendor a second quickjs-ng.
  exit /b 2
)
set "QJS_OUT=%WASIGO_ROOT%\build\wasigocvm-quickjs"
set "QJS_STAMP="
if exist "%QJS_OUT%\qjs.src" set /p QJS_STAMP=<"%QJS_OUT%\qjs.src"
if not "!QJS_STAMP!"=="!QJS_SRC!" if exist "%QJS_OUT%" rmdir /s /q "%QJS_OUT%"
if not exist "%QJS_OUT%" mkdir "%QJS_OUT%"
> "%QJS_OUT%\qjs.src" echo(!QJS_SRC!
set "WASIGO_CC=%WASIGO_CLANG:clang++=clang%"
set WASIGO_QJS_O=
for %%F in (quickjs libregexp libunicode dtoa quickjs-libc) do (
  if not exist "%QJS_OUT%\%%F.o" (
    "!WASIGO_CC!" -c -O2 -std=gnu11 -funsigned-char -fvisibility=hidden -mllvm -wasm-enable-sjlj -DQJS_BUILD_LIBC -DQUICKJS_NG_BUILD -D_GNU_SOURCE -D_REENTRANT -D_WASI_EMULATED_GETPID -D_WASI_EMULATED_MMAN -D_WASI_EMULATED_PROCESS_CLOCKS -D_WASI_EMULATED_SIGNAL -isystem "%WASIGO_SYSROOT%\include\%WASIGO_TRIPLE%" -isystem "%WASIGO_SYSROOT%\include" -I "%QJS_SRC%" -o "%QJS_OUT%\%%F.o" "%QJS_SRC%\%%F.c"
    if errorlevel 1 (
      echo quickjs-ng %%F failed
      exit /b 1
    )
  )
  set WASIGO_QJS_O=!WASIGO_QJS_O! "%QJS_OUT%\%%F.o"
)
echo             quickjs-ng from WASMv8bindings ^(!QJS_SRC!^)
set "GOCLC_WRAP=-Wl,--wrap=fopen -Wl,--wrap=fclose -Wl,--wrap=fread -Wl,--wrap=fwrite -Wl,--wrap=fflush -Wl,--wrap=fseek -Wl,--wrap=ftell -Wl,--wrap=fseeko -Wl,--wrap=ftello -Wl,--wrap=fileno -Wl,--wrap=fdopen -Wl,--wrap=feof -Wl,--wrap=ferror -Wl,--wrap=clearerr -Wl,--wrap=rewind -Wl,--wrap=stat -Wl,--wrap=lstat -Wl,--wrap=fstat -Wl,--wrap=mkdir -Wl,--wrap=rmdir -Wl,--wrap=unlink -Wl,--wrap=remove -Wl,--wrap=rename -Wl,--wrap=access -Wl,--wrap=getcwd -Wl,--wrap=chdir -Wl,--wrap=opendir -Wl,--wrap=readdir -Wl,--wrap=closedir -Wl,--wrap=pipe -Wl,--wrap=dup -Wl,--wrap=dup2"
rem One response file: the source list is longer than cmd.exe takes on a
rem command line (8191). It sits beside the generated C++.
set "RSP=%GO_DIR%\%GO_BASE%_gen.rsp"
> "%RSP%" echo(-O2 -std=c++20 -fuse-ld=lld -fexceptions -frtti -fwasm-exceptions -mllvm -wasm-use-legacy-eh=false -mllvm -wasm-enable-sjlj -nostdinc++
>> "%RSP%" echo(-isystem "%EH_INC%" -isystem "%WASIGO_SYSROOT%\include\%WASIGO_TRIPLE%" -isystem "%WASIGO_SYSROOT%\include" -L "%EH_LIB%" -lc++ -lc++abi -lunwind
>> "%RSP%" echo(-Wl,--export=__indirect_function_table %GOCLC_WRAP% %WASIGO_PTHREAD_FLAGS%
>> "%RSP%" echo(-I "%GO_DIR%" -I "%WASIGO_SRC_INC%" !WASIGO_TURBO_I! !WASIGO_WIN32_I! !WASIGO_NIX_I! !WASIGO_DROID_I! !WASIGO_GOCOS_I! !WASIGO_WNS_I! !WASIGO_WINE_I! !WASIGO_CHROME_I! !WASIGO_PE_I! !WASIGO_SAFE_I! !WASIGO_V8_I! !WASIGO_SSL_I! !WASIGO_REACT_I! !WASIGO_WEBGPU_I! -I "%QJS_SRC%"
>> "%RSP%" echo(-DWASIGO_GOCVM=1 !WASIGO_CHROME_D! !WASIGO_GOCOS_D! !WASIGO_WEBGPU_D! !WASIGO_SSL_D! !WASIGO_REACT_D! -D_WASI_EMULATED_MMAN -D_WASI_EMULATED_GETPID -D_WASI_EMULATED_SIGNAL -D_WASI_EMULATED_PROCESS_CLOCKS -D_REENTRANT
>> "%RSP%" echo(-lwasi-emulated-mman -lwasi-emulated-signal -lwasi-emulated-process-clocks -lsetjmp -o "%OUTWASM%"
>> "%RSP%" echo("%GENCPP%" "%WASIGO_ROOT%\libc\goclibc.c" "%WASIGO_SYSROOT%\src\getpid.c"
>> "%RSP%" echo(!WASIGO_TURBO_CC!
>> "%RSP%" echo(!WASIGO_WIN32_CC! !WASIGO_NIX_CC! !WASIGO_DROID_CC!
>> "%RSP%" echo(!WASIGO_GOCOS_CC! !WASIGO_WINE_CC!
>> "%RSP%" echo(!WASIGO_WNS_CC!
>> "%RSP%" echo(!WASIGO_V8F_CC! !WASIGO_QJS_O!
>> "%RSP%" echo(!WASIGO_CHROME_CC! !WASIGO_SAFE_CC! !WASIGO_V8_CC! !WASIGO_REACT_CC! !WASIGO_WEBGPU_CC! !WASIGO_SSL_L!
"%WASIGO_CLANG%" --rsp-quoting=windows @"%RSP%"
if errorlevel 1 (
  echo wasigocvm clang++ failed
  exit /b 1
)
echo.
echo wasm: %OUTWASM%
echo run:  wasitime "%OUTWASM%"
exit /b 0

:usage
echo usage: wasigocvm.bat ^<input.go^> [-o out.wasm] [wasigoc flags...]
echo.
echo Same machine as compile.bat ^(the default^). Full libc++, RTTI, WASM EH,
echo Oilpan + type_key, gocvm net. Prefers WASIGO_TOOLCHAIN / toolchain\sysroot.
echo Isolation is WASMSafeSpace in this module. No companion gocvm_host.
echo Stock wasip1 noeh leftover: legacy.bat
exit /b 0

:find_wasigoc
set "WASIGOC_EXE="
if exist "build\%CONFIG%\wasigoc.exe" set "WASIGOC_EXE=%cd%\build\%CONFIG%\wasigoc.exe"
if not defined WASIGOC_EXE if exist "build\wasigoc.exe" set "WASIGOC_EXE=%cd%\build\wasigoc.exe"
if not defined WASIGOC_EXE if exist "build\Release\wasigoc.exe" set "WASIGOC_EXE=%cd%\build\Release\wasigoc.exe"
if not defined WASIGOC_EXE if exist "build-fork\wasigoc.exe" set "WASIGOC_EXE=%cd%\build-fork\wasigoc.exe"
if not defined WASIGOC_EXE if exist "wasigoc.exe" set "WASIGOC_EXE=%cd%\wasigoc.exe"
exit /b 0

:find_toolchain
set "WASIGO_CLANG="
set "WASIGO_SYSROOT="
set "WASIGO_SDK_ROOT="
rem 1) Custom wasigocvm sysroot
if defined WASIGO_TOOLCHAIN if exist "%WASIGO_TOOLCHAIN%\sysroot\include" (
  set "WASIGO_SYSROOT=%WASIGO_TOOLCHAIN%\sysroot"
  set "WASIGO_SDK_ROOT=%WASIGO_TOOLCHAIN%"
)
if not defined WASIGO_SYSROOT if exist "%~dp0toolchain\sysroot\include" (
  set "WASIGO_SYSROOT=%~dp0toolchain\sysroot"
  set "WASIGO_SDK_ROOT=%~dp0toolchain"
)
rem share\wasi-sysroot layout from bootstrap install prefix
if not defined WASIGO_SYSROOT if defined WASIGO_TOOLCHAIN if exist "%WASIGO_TOOLCHAIN%\share\wasi-sysroot\include" (
  set "WASIGO_SYSROOT=%WASIGO_TOOLCHAIN%\share\wasi-sysroot"
  set "WASIGO_SDK_ROOT=%WASIGO_TOOLCHAIN%"
)
rem 2) Stock wasi-sdk borrow
if not defined WASIGO_SYSROOT if defined WASI_SDK_PATH if exist "%WASI_SDK_PATH%\share\wasi-sysroot\include" (
  set "WASIGO_SYSROOT=%WASI_SDK_PATH%\share\wasi-sysroot"
  set "WASIGO_SDK_ROOT=%WASI_SDK_PATH%"
)
if not defined WASIGO_SYSROOT if exist "%USERPROFILE%\wasi-sdk\share\wasi-sysroot\include" (
  set "WASIGO_SYSROOT=%USERPROFILE%\wasi-sdk\share\wasi-sysroot"
  set "WASIGO_SDK_ROOT=%USERPROFILE%\wasi-sdk"
)
if not defined WASIGO_SYSROOT exit /b 0
call :pick_clang "%WASIGO_SDK_ROOT%"
rem A bootstrap.sh --target install run (the default) has a real sysroot
rem but no bin\ of its own (see toolchain\README.md) -- pair that sysroot
rem with the stock wasi-sdk's clang++ instead of giving up.
if not defined WASIGO_CLANG if defined WASI_SDK_PATH (
  call :pick_clang "%WASI_SDK_PATH%"
  if defined WASIGO_CLANG echo [wasigocvm] pairing %WASIGO_SYSROOT% with clang++ from %WASI_SDK_PATH% ^(no bin\ of its own^)
)
if not defined WASIGO_CLANG if exist "%USERPROFILE%\wasi-sdk\bin" (
  call :pick_clang "%USERPROFILE%\wasi-sdk"
  if defined WASIGO_CLANG echo [wasigocvm] pairing %WASIGO_SYSROOT% with clang++ from %USERPROFILE%\wasi-sdk ^(no bin\ of its own^)
)
exit /b 0

:pick_clang
set "WASIGO_CLANG="
if exist "%~1\bin\wasm32-wasigocvm-clang++.exe" set "WASIGO_CLANG=%~1\bin\wasm32-wasigocvm-clang++.exe"
if not defined WASIGO_CLANG if exist "%~1\bin\wasm32-wasigocvm-clang++" set "WASIGO_CLANG=%~1\bin\wasm32-wasigocvm-clang++"
if not defined WASIGO_CLANG if exist "%~1\bin\wasm32-wasip2-clang++.exe" set "WASIGO_CLANG=%~1\bin\wasm32-wasip2-clang++.exe"
if not defined WASIGO_CLANG if exist "%~1\bin\wasm32-wasip2-clang++" set "WASIGO_CLANG=%~1\bin\wasm32-wasip2-clang++"
exit /b 0

:pick_triple
set "WASIGO_TRIPLE="
if exist "%WASIGO_SYSROOT%\include\wasm32-wasigocvm\eh\c++\v1\iostream" set "WASIGO_TRIPLE=wasm32-wasigocvm"
if not defined WASIGO_TRIPLE if exist "%WASIGO_SYSROOT%\include\wasm32-wasip2\eh\c++\v1\iostream" set "WASIGO_TRIPLE=wasm32-wasip2"
exit /b 0
