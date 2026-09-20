cmake_minimum_required(VERSION 3.16)
project(WASMWin32 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# wasmwin32: a wasigocvm-facing projection of public Win32 metadata
# (https://github.com/microsoft/win32metadata) plus WSL
# (https://github.com/microsoft/WSL). Native Windows calls kernel32/wsl.exe.
# wasm32 / POSIX uses libc (getpid, getenv, uname, stat, clocks) plus
# std::thread CreateProcessW — same wasmwin32_call names, no companion
# host drop. LoadLibrary is kernel32 on native and the in-module
# catalog/file probe on wasm (PE via ~/WASMPELoader). Native leftover also
# links ntdll (GetProcAddress), bcrypt, and ncrypt.
add_library(wasmwin32 STATIC
  src/catalog.cc
)
if(WIN32 AND NOT CMAKE_SYSTEM_NAME STREQUAL "WASI")
  target_sources(wasmwin32 PRIVATE src/host_win.cc)
  target_link_libraries(wasmwin32 PRIVATE advapi32 psapi user32 gdi32 ole32 oleaut32 ws2_32 bcrypt ncrypt crypt32 shell32 shlwapi winhttp iphlpapi version comctl32 wininet dnsapi secur32 dbghelp wintrust uxtheme dwmapi rpcrt4 setupapi cfgmgr32 netapi32 pdh wevtapi comdlg32)
else()
  target_sources(wasmwin32 PRIVATE src/host_wasi.cc)
endif()
set(WASMPELOADER_DIR "$ENV{USERPROFILE}/WASMPELoader")
if(NOT EXISTS "${WASMPELOADER_DIR}/include/wasmpe/loader.hpp")
  set(WASMPELOADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../WASMPELoader")
endif()
if(NOT EXISTS "${WASMPELOADER_DIR}/include/wasmpe/loader.hpp")
  message(FATAL_ERROR "WASMPELoader not found (expected ~/WASMPELoader)")
endif()
target_include_directories(wasmwin32 PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${WASMPELOADER_DIR}/include
)

if(MSVC)
  target_compile_options(wasmwin32 PRIVATE /W4)
else()
  target_compile_options(wasmwin32 PRIVATE -Wall -Wextra)
endif()

enable_testing()

add_executable(win32_test tests/win32_test.cc)
target_link_libraries(win32_test PRIVATE wasmwin32)
add_test(NAME win32_test COMMAND win32_test)

add_executable(wsl_test tests/wsl_test.cc)
target_link_libraries(wsl_test PRIVATE wasmwin32)
add_test(NAME wsl_test COMMAND wsl_test)

add_executable(win32_example examples/hello/main.cc)
target_link_libraries(win32_example PRIVATE wasmwin32)

add_custom_target(wasmwin32_everything DEPENDS
  wasmwin32 win32_test wsl_test win32_example
)
