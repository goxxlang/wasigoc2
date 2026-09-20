@echo off
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"

rem compile.bat — default is the wasigocvm machine (-DWASIGO_GOCVM=1).
rem Stock wasm32-wasip1 noeh leftover: legacy.bat
rem
rem   compile.bat                         build wasigoc.exe
rem   compile.bat examples\hello\hello.go wasigocvm wasm
rem   compile.bat hello.go -o out.wasm
rem   legacy.bat hello.go                 stock wasip1 noeh

if /I "%~1"=="-h" goto :usage
if /I "%~1"=="--help" goto :usage
if /I "%~1"=="/?" goto :usage

set "HASGO="
for %%A in (%*) do (
  if /I "%%~A"=="--wasip2" (
    echo error: --wasip2 is retired. compile.bat is already wasigocvm.
    echo        see docs\wasigocvm.md
    exit /b 2
  )
  if /I "%%~xA"==".go" set "HASGO=1"
)

if defined HASGO (
  call "%~dp0wasigocvm.bat" %*
  exit /b %ERRORLEVEL%
)

call "%~dp0legacy.bat" %*
exit /b %ERRORLEVEL%

:usage
echo usage: compile.bat [Debug^|Release] [^<input.go^> [-o out.wasm] [wasigoc flags...]]
echo.
echo   Default compile is wasigocvm ^(full libc++ / EH / RTTI / WASIGO_GOCVM^).
echo.
echo   no .go file   build the host compiler wasigoc.exe
echo   input.go      transpile + wasigocvm clang++  ^(docs\wasigocvm.md^)
echo.
echo Stock wasip1 noeh leftover: legacy.bat
echo Alias: wasigocvm.bat ^(same machine as compile.bat with a .go file^)
exit /b 0
