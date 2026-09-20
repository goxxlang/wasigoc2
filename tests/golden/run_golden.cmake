# Compiles one wasigoc-generated .cpp and validates at ctest time.
#
# Required: CLANGXX, CPP_FILE, WASM_FILE
# Optional: SYSROOT, WASMTIME, EXPECTED_OUTPUT, WASI_PREVIEW (p1|p2),
#           SRC_DIR, WASMTIME_WASI, WASIGO_CXXLIB (noeh|full),
#           EXTRA_DEFINES (semicolon list, each becomes -D<item>)
#
# WASIGO_CXXLIB=full is the wasigocvm contract: eh libc++, RTTI, standard
# WASM exceptions (not legacy). Default remains noeh for stock p1 goldens.

if(NOT DEFINED CLANGXX OR NOT DEFINED CPP_FILE OR NOT DEFINED WASM_FILE)
  message(FATAL_ERROR "run_golden.cmake: CLANGXX, CPP_FILE, and WASM_FILE are required")
endif()

if(NOT DEFINED SYSROOT OR SYSROOT STREQUAL "")
  get_filename_component(_clang_bin "${CLANGXX}" DIRECTORY)
  set(SYSROOT "${_clang_bin}/../share/wasi-sysroot")
endif()

if(NOT DEFINED WASI_PREVIEW OR WASI_PREVIEW STREQUAL "")
  set(WASI_PREVIEW p1)
endif()
if(WASI_PREVIEW STREQUAL "p2")
  # Prefer an explicit wasigocvm triple layout if the sysroot has one.
  if(EXISTS "${SYSROOT}/include/wasm32-wasigocvm/eh/c++/v1/iostream")
    set(_triple wasm32-wasigocvm)
  else()
    set(_triple wasm32-wasip2)
  endif()
else()
  set(_triple wasm32-wasip1)
endif()

if(NOT DEFINED WASIGO_CXXLIB OR WASIGO_CXXLIB STREQUAL "")
  if(WASI_PREVIEW STREQUAL "p2")
    set(WASIGO_CXXLIB full)
  else()
    set(WASIGO_CXXLIB noeh)
  endif()
endif()

get_filename_component(_clang_name "${CLANGXX}" NAME)
set(_target_flag)
if(NOT _clang_name MATCHES "wasm32-")
  set(_target_flag --target=${_triple})
endif()

set(_include_flag)
if(DEFINED INCLUDE_DIR AND NOT INCLUDE_DIR STREQUAL "")
  set(_include_flag -I "${INCLUDE_DIR}")
endif()
if(DEFINED SRC_DIR AND NOT SRC_DIR STREQUAL "")
  list(APPEND _include_flag -I "${SRC_DIR}")
endif()

set(_cxx_flags -O2 -std=c++20)
set(_link_flags)
set(_isystem_flags)
set(_extra_cc)
if(WASIGO_CXXLIB STREQUAL "full")
  # wasigocvm: full libc++, RTTI, standard (non-legacy) WASM EH
  list(APPEND _cxx_flags -fexceptions -frtti -fwasm-exceptions
       -mllvm -wasm-use-legacy-eh=false -DWASIGO_GOCVM=1
       -D_WASI_EMULATED_MMAN -D_WASI_EMULATED_GETPID)
  set(_isystem_flags
    -nostdinc++
    -isystem "${SYSROOT}/include/${_triple}/eh/c++/v1"
    -isystem "${SYSROOT}/include/${_triple}"
    -isystem "${SYSROOT}/include"
  )
  list(APPEND _link_flags
    -L "${SYSROOT}/lib/${_triple}/eh"
    -lc++ -lc++abi -lunwind
    -lwasi-emulated-mman
    -Wl,--export=__indirect_function_table
  )
  # Host must grant -W exceptions (see check_wasm.cmake WASMTIME_WASM)
  if(NOT DEFINED WASMTIME_WASM OR WASMTIME_WASM STREQUAL "")
    set(WASMTIME_WASM "exceptions")
  endif()
  if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../../toolchain/openssl-wasm/include/openssl/ssl.h"
     AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/../../toolchain/openssl-wasm/lib/libssl.a")
    list(APPEND _include_flag -I "${CMAKE_CURRENT_LIST_DIR}/../../toolchain/openssl-wasm/include")
    list(APPEND _cxx_flags -DWASIGO_HAS_OPENSSL=1)
    list(APPEND _link_flags
      "${CMAKE_CURRENT_LIST_DIR}/../../toolchain/openssl-wasm/lib/libssl.a"
      "${CMAKE_CURRENT_LIST_DIR}/../../toolchain/openssl-wasm/lib/libcrypto.a")
  endif()
  # Exec second address space: EPT/TPT (WASMSafeSpace) + CHPT (WASMv8bindings).
  set(_home "$ENV{USERPROFILE}")
  if(_home STREQUAL "")
    set(_home "$ENV{HOME}")
  endif()
  set(_wss "${CMAKE_CURRENT_LIST_DIR}/../../WASMSafeSpace")
  if(NOT EXISTS "${_wss}/src/sandbox/sandbox.cc")
    set(_wss "${_home}/WASMSafeSpace")
  endif()
  if(NOT EXISTS "${_wss}/src/sandbox/sandbox.cc")
    set(_wss "${CMAKE_CURRENT_LIST_DIR}/../../../WASMSafeSpace")
  endif()
  set(_wv8 "${CMAKE_CURRENT_LIST_DIR}/../../WASMv8bindings")
  if(NOT EXISTS "${_wv8}/include/cppgc/heap.h")
    set(_wv8 "${_home}/WASMv8bindings")
  endif()
  if(NOT EXISTS "${_wv8}/include/cppgc/heap.h")
    set(_wv8 "${_home}/WASMv8Bindings")
  endif()
  if(NOT EXISTS "${_wv8}/include/cppgc/heap.h")
    set(_wv8 "${CMAKE_CURRENT_LIST_DIR}/../../../WASMv8bindings")
  endif()
  if(EXISTS "${_wss}/src/sandbox/sandbox.cc")
    list(APPEND _include_flag -I "${_wss}/include" -I "${_wss}")
    list(APPEND _extra_cc
      "${_wss}/src/sandbox/sandbox.cc"
      "${_wss}/src/sandbox/external-pointer-table.cc"
      "${_wss}/src/sandbox/trusted-pointer-table.cc")
  endif()
  if(EXISTS "${_wv8}/include/cppgc/heap.h")
    list(APPEND _include_flag -I "${_wv8}/include" -I "${_wv8}")
    list(APPEND _extra_cc
      "${_wv8}/src/heap/internal/cppgc-logging.cc"
      "${_wv8}/src/heap/internal/free-list.cc"
      "${_wv8}/src/heap/internal/gc-info.cc"
      "${_wv8}/src/heap/internal/gc-info-table.cc"
      "${_wv8}/src/heap/internal/heap.cc"
      "${_wv8}/src/heap/internal/platform.cc"
      "${_wv8}/src/sandbox/cppheap-pointer-table.cc")
  endif()
  set(_w32 "${CMAKE_CURRENT_LIST_DIR}/../../WASMWin32")
  if(NOT EXISTS "${_w32}/include/win32/wasi_host.hpp")
    set(_w32 "${_home}/WASMWin32")
  endif()
  if(NOT EXISTS "${_w32}/include/win32/wasi_host.hpp")
    set(_w32 "${CMAKE_CURRENT_LIST_DIR}/../../../WASMWin32")
  endif()
  if(EXISTS "${_w32}/include/win32/wasi_host.hpp")
    list(APPEND _include_flag -I "${_w32}/include")
    if(EXISTS "${_w32}/src/catalog.cc")
      list(APPEND _extra_cc "${_w32}/src/catalog.cc")
    endif()
  endif()
  set(_nix "${CMAKE_CURRENT_LIST_DIR}/../../WASMNix")
  if(NOT EXISTS "${_nix}/include/nix/posix_host.hpp")
    set(_nix "${_home}/WASMNix")
  endif()
  if(NOT EXISTS "${_nix}/include/nix/posix_host.hpp")
    set(_nix "${CMAKE_CURRENT_LIST_DIR}/../../../WASMNix")
  endif()
  if(EXISTS "${_nix}/include/nix/posix_host.hpp")
    list(APPEND _include_flag -I "${_nix}/include")
    if(EXISTS "${_nix}/src/catalog.cc")
      list(APPEND _extra_cc "${_nix}/src/catalog.cc")
    endif()
  endif()
  set(_droid "${CMAKE_CURRENT_LIST_DIR}/../../WASMDroid")
  if(NOT EXISTS "${_droid}/include/droid/bionic_host.hpp")
    set(_droid "${_home}/WASMDroid")
  endif()
  if(NOT EXISTS "${_droid}/include/droid/bionic_host.hpp")
    set(_droid "${CMAKE_CURRENT_LIST_DIR}/../../../WASMDroid")
  endif()
  if(EXISTS "${_droid}/include/droid/bionic_host.hpp")
    list(APPEND _include_flag -I "${_droid}/include")
    if(EXISTS "${_droid}/src/catalog.cc")
      list(APPEND _extra_cc "${_droid}/src/catalog.cc")
    endif()
  endif()
  set(_gocos "${CMAKE_CURRENT_LIST_DIR}/../../WASMGocOS")
  if(NOT EXISTS "${_gocos}/include/gocos/host.hpp")
    set(_gocos "${_home}/WASMGocOS")
  endif()
  if(NOT EXISTS "${_gocos}/include/gocos/host.hpp")
    set(_gocos "${CMAKE_CURRENT_LIST_DIR}/../../../WASMGocOS")
  endif()
  if(EXISTS "${_gocos}/include/gocos/host.hpp")
    list(APPEND _include_flag -I "${_gocos}/include")
    if(EXISTS "${_gocos}/src/catalog.cc")
      list(APPEND _extra_cc "${_gocos}/src/catalog.cc")
    endif()
  endif()
  set(_pe "${CMAKE_CURRENT_LIST_DIR}/../../WASMPELoader")
  if(NOT EXISTS "${_pe}/include/wasmpe/loader.hpp")
    set(_pe "${_home}/WASMPELoader")
  endif()
  if(NOT EXISTS "${_pe}/include/wasmpe/loader.hpp")
    set(_pe "${CMAKE_CURRENT_LIST_DIR}/../../../WASMPELoader")
  endif()
  if(EXISTS "${_pe}/include/wasmpe/loader.hpp")
    list(APPEND _include_flag -I "${_pe}/include")
  endif()
else()
  list(APPEND _cxx_flags -fno-exceptions)
  set(_isystem_flags
    -nostdinc++
    -isystem "${SYSROOT}/include/${_triple}/noeh/c++/v1"
    -isystem "${SYSROOT}/include/${_triple}"
    -isystem "${SYSROOT}/include"
  )
endif()

if(DEFINED EXTRA_DEFINES AND NOT EXTRA_DEFINES STREQUAL "")
  foreach(_d IN LISTS EXTRA_DEFINES)
    if(NOT _d STREQUAL "")
      list(APPEND _cxx_flags "-D${_d}")
    endif()
  endforeach()
endif()

execute_process(
  COMMAND "${CLANGXX}" ${_target_flag}
          ${_cxx_flags}
          ${_isystem_flags}
          ${_include_flag}
          ${_link_flags}
          -o "${WASM_FILE}" "${CPP_FILE}" ${_extra_cc}
  OUTPUT_VARIABLE compile_output
  ERROR_VARIABLE compile_error
  RESULT_VARIABLE compile_result
)
if(NOT compile_result EQUAL 0)
  message(FATAL_ERROR
    "wasi-sdk clang++ failed (${_triple}, cxxlib=${WASIGO_CXXLIB}):\n${compile_output}\n${compile_error}")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/check_wasm.cmake")
