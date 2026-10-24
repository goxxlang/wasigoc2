cmake_minimum_required(VERSION 3.16)
project(WASMGocOS LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# MinGW: link the GCC runtime (libstdc++, libgcc, winpthread) into each
# binary. Linked dynamically, a test loads whichever libstdc++-6.dll comes
# first on PATH -- Git for Windows ships an older one in /mingw64/bin --
# and runs against a C++ runtime it was not built with.
if(MINGW)
  add_link_options(-static-libgcc -static-libstdc++ -static)
endif()

# go++'s llvm-ar is a Linux binary, so the wasm build has no archiver.
# Every static library here and in the siblings (TurboSpace, SafeSpace,
# WASMReact, quickjs, WASMNetStack) is an OBJECT library instead, and
# gocos_wasi_link hands a module their objects between --start-lib and
# --end-lib: wasm-ld takes an object from that group only when something
# needs a symbol from it, the way it takes an archive member.
set(GOCOS_WASI FALSE)
if(CMAKE_SYSTEM_NAME STREQUAL "WASI")
  set(GOCOS_WASI TRUE)
  get_property(_gocos_add_library_wrapped GLOBAL PROPERTY GOCOS_ADD_LIBRARY_WRAPPED)
  if(NOT _gocos_add_library_wrapped)
    set_property(GLOBAL PROPERTY GOCOS_ADD_LIBRARY_WRAPPED TRUE)
    function(add_library name)
      set(_args ${ARGN})
      set(_kind "")
      if(_args)
        list(GET _args 0 _kind)
      endif()
      if(_kind STREQUAL "STATIC")
        list(REMOVE_AT _args 0)
      elseif(_kind MATCHES "^(SHARED|MODULE|OBJECT|INTERFACE|UNKNOWN|ALIAS|IMPORTED)$")
        _add_library(${name} ${ARGN})
        return()
      endif()
      _add_library(${name} OBJECT ${_args})
      set_property(GLOBAL APPEND PROPERTY GOCOS_WASI_OBJECT_LIBS ${name})
    endfunction()
  endif()
endif()

# The module's link line: every recorded OBJECT library as one lazy group,
# in a wasm-ld response file (the object list is longer than cmd.exe takes).
function(gocos_wasi_link target)
  if(NOT GOCOS_WASI)
    return()
  endif()
  get_property(_libs GLOBAL PROPERTY GOCOS_WASI_OBJECT_LIBS)
  set(_objs "")
  foreach(_lib IN LISTS _libs)
    if(NOT _lib STREQUAL "wasmgocos" AND TARGET ${_lib})
      list(APPEND _objs "$<TARGET_OBJECTS:${_lib}>")
      add_dependencies(${target} ${_lib})
    endif()
  endforeach()
  set(_rsp "${CMAKE_CURRENT_BINARY_DIR}/${target}.lazy.rsp")
  file(GENERATE OUTPUT "${_rsp}"
    CONTENT "--start-lib\n$<JOIN:${_objs},\n>\n--end-lib\n")
  # -Wl, so clang hands "@file" to wasm-ld instead of expanding it itself.
  target_link_options(${target} PRIVATE "-Wl,@${_rsp}")
  set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS ${_objs})
endfunction()

# wasigocvm module: go++'s sysroot and goclibc, the same compile and link
# line as wasigocvm.bat. --sysroot here overrides clang++.cfg's stock
# share/wasi-sysroot. Directory-wide, so the siblings compiled below get
# the same sysroot, sjlj lowering, and emulated libc as the kernel.
set(_gocos_sib "${CMAKE_CURRENT_SOURCE_DIR}/..")
if(GOCOS_WASI)
  if(NOT WASIGO_SYSROOT)
    message(FATAL_ERROR "configure with -DCMAKE_TOOLCHAIN_FILE=go++/toolchain/share/cmake/wasigocvm.cmake")
  endif()
  set(_gocos_goclibc "${_gocos_sib}/go++/libc/goclibc.c")
  if(NOT EXISTS "${_gocos_goclibc}")
    message(FATAL_ERROR "goclibc not found: ${_gocos_goclibc}")
  endif()
  enable_language(C)
  # The compiler probe ran under clang.cfg's stock share/wasi-sysroot and
  # records its lib dirs as implicit -L paths. Only go++'s sysroot links.
  foreach(_lang C CXX)
    list(FILTER CMAKE_${_lang}_IMPLICIT_LINK_DIRECTORIES EXCLUDE REGEX "/share/wasi-sysroot/")
  endforeach()
  add_compile_definitions(_WASI_EMULATED_MMAN _WASI_EMULATED_GETPID _WASI_EMULATED_SIGNAL
    _WASI_EMULATED_PROCESS_CLOCKS _REENTRANT)
  add_compile_options("--sysroot=${WASIGO_SYSROOT}" "SHELL:-mllvm -wasm-enable-sjlj")
  set(_gocos_goclc_wrap fopen fclose fread fwrite fflush fseek ftell fseeko ftello fileno fdopen
    feof ferror clearerr rewind stat lstat fstat mkdir rmdir unlink remove rename
    access getcwd chdir opendir readdir closedir pipe dup dup2)
  list(TRANSFORM _gocos_goclc_wrap PREPEND "-Wl,--wrap=")
  add_link_options("--sysroot=${WASIGO_SYSROOT}" -fuse-ld=lld ${_gocos_goclc_wrap})
  link_libraries(wasi-emulated-mman wasi-emulated-getpid wasi-emulated-signal
    wasi-emulated-process-clocks setjmp)
endif()

# wasmgocos: wasigocvm edge kernel. Hypervision attaches TurboSpace's
# vCPU (one WHv partition, VM window) on native Windows. k32 / nix stay
# the syscall catalogs. Native and wasm use host.hpp. host_win.cc is not
# linked into wasm. Under WASI it is an OBJECT library, linked into each
# module directly, as wasigocvm.bat does.
if(GOCOS_WASI)
  set(_gocos_lib_kind OBJECT)
else()
  set(_gocos_lib_kind STATIC)
endif()
add_library(wasmgocos ${_gocos_lib_kind}
  src/catalog.cc
  src/unil.cc
)
if(WIN32 AND NOT GOCOS_WASI)
  target_sources(wasmgocos PRIVATE src/host_win.cc)
elseif(GOCOS_WASI)
  target_sources(wasmgocos PRIVATE src/host_wasi.cc)
else()
  target_sources(wasmgocos PRIVATE src/host_linux.cc)
endif()

target_include_directories(wasmgocos PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
# WASMWine: kernel32/ntdll PE images + stdcall signatures for peinterp.
if(EXISTS "${_gocos_sib}/WASMWine/include/wine/dll.h")
  target_include_directories(wasmgocos PUBLIC "${_gocos_sib}/WASMWine/include")
endif()

# wasigocvm / POSIX guests compile wasi_host.hpp and posix_host.hpp.
# Native MSVC cannot parse those POSIX headers; the driver adds -I.
if(NOT (WIN32 AND NOT GOCOS_WASI))
  if(EXISTS "${_gocos_sib}/WASMWin32/include/win32/wasi_host.hpp")
    target_include_directories(wasmgocos PUBLIC "${_gocos_sib}/WASMWin32/include")
  endif()
  if(EXISTS "${_gocos_sib}/WASMnix/include/nix/posix_host.hpp")
    target_include_directories(wasmgocos PUBLIC "${_gocos_sib}/WASMnix/include")
  endif()
  if(EXISTS "${_gocos_sib}/WASMPELoader/include/wasmpe/loader.hpp")
    target_include_directories(wasmgocos PUBLIC "${_gocos_sib}/WASMPELoader/include")
  endif()
endif()

if(GOCOS_WASI)
  target_sources(wasmgocos PRIVATE "${_gocos_goclibc}")
  target_compile_definitions(wasmgocos PUBLIC WASIGO_GOCVM=1)
endif()

# TurboSpace is the kernel runtime: the cage, the IPT, the origin
# filesystem, and the engine every guest runs on. It is also the vCPU
# on native and on wasm. k32 is a vCPU exit either way. The wasm module
# has no WinHvPlatform.lib; its partition is the WinHvPlatform DLL
# catalog (src/vcpu.cc), the same wasmturbo_vcpu_* symbols.
if(WIN32 OR GOCOS_WASI)
  set(WTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../WASMTurboSpace")
  if(NOT TARGET wasmturbo_engine AND EXISTS "${WTS_DIR}/CMakeLists.txt")
    add_subdirectory("${WTS_DIR}" "${CMAKE_BINARY_DIR}/wasmturbo" EXCLUDE_FROM_ALL)
  endif()
  if(TARGET wasmturbo_engine)
    target_sources(wasmgocos PRIVATE src/jitrt.cc src/bbx.cc src/session.cc src/sh.cc src/ctr.cc src/svc.cc
      src/react.cc src/peinterp.cc src/wasi.cc src/files.cc src/klock.cc src/stream.cc)
    if(EXISTS "${_gocos_sib}/WASMWine/src/dll.cc")
      target_sources(wasmgocos PRIVATE "${_gocos_sib}/WASMWine/src/dll.cc")
    endif()
    # FreeLoader and ntoskrnl. Not add_subdirectory: that tree defines
    # boot_test and a second copy of the cage. These sources compile
    # against the go++ tables they were written for.
    set(REACT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../WASMReact")
    if(EXISTS "${REACT_DIR}/src/host.cc" AND NOT TARGET wasmreact_kernel)
      add_library(wasmreact_kernel STATIC
        "${REACT_DIR}/src/rtl/rtl.cc"
        "${REACT_DIR}/src/hal/halwasm.cc"
        "${REACT_DIR}/src/hal/hv.cc"
        "${REACT_DIR}/src/inbv/inbv.cc"
        "${REACT_DIR}/src/ke/aspace.cc"
        "${REACT_DIR}/src/ke/bug.cc"
        "${REACT_DIR}/src/ke/dispatcher.cc"
        "${REACT_DIR}/src/ke/kiinit.cc"
        "${REACT_DIR}/src/mm/mminit.cc"
        "${REACT_DIR}/src/mm/expool.cc"
        "${REACT_DIR}/src/ex/handle.cc"
        "${REACT_DIR}/src/ex/init.cc"
        "${REACT_DIR}/src/ex/exobj.cc"
        "${REACT_DIR}/src/ob/obtype.cc"
        "${REACT_DIR}/src/ob/oblife.cc"
        "${REACT_DIR}/src/ob/obdir.cc"
        "${REACT_DIR}/src/ob/obname.cc"
        "${REACT_DIR}/src/ob/obhandle.cc"
        "${REACT_DIR}/src/ob/oblink.cc"
        "${REACT_DIR}/src/ob/obinit.cc"
        "${REACT_DIR}/src/se/semgr.cc"
        "${REACT_DIR}/src/ps/psmgr.cc"
        "${REACT_DIR}/src/io/iomgr.cc"
        "${REACT_DIR}/src/drivers/base/null/null.cc"
        "${REACT_DIR}/src/config/cm.cc"
        "${REACT_DIR}/src/boot/freeldr/machwasm.cc"
        "${REACT_DIR}/src/boot/freeldr/winldr.cc"
        "${REACT_DIR}/src/catalog.cc"
        "${REACT_DIR}/src/host.cc"
      )
      target_include_directories(wasmreact_kernel PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../WASMVoodooCompile/WASMv8bindings/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../WASMVoodooCompile/WASMv8bindings"
        "${CMAKE_CURRENT_SOURCE_DIR}/../WASMVoodooCompile/WASMSafeSpace/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../WASMVoodooCompile/WASMSafeSpace"
        "${REACT_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../WASMWin32/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../WASMPELoader/include"
      )
      target_compile_definitions(wasmreact_kernel PRIVATE WASMREACT_BUILD_DATE="gocos")
      if(NOT GOCOS_WASI)
        target_compile_definitions(wasmreact_kernel PRIVATE WASMGOCOS_K32_NATIVE=1)
      endif()
      target_include_directories(wasmgocos PRIVATE "${REACT_DIR}/include")
      target_link_libraries(wasmgocos PUBLIC wasmreact_kernel)
    endif()
    target_link_libraries(wasmgocos PUBLIC wasmturbo_engine)
    # JITRT: the kernel services (session, shell, containers, guests).
    # VCPU: the WHv partition under them, native and wasm.
    target_compile_definitions(wasmgocos PUBLIC WASMGOCOS_HAS_JITRT=1 WASMGOCOS_HAS_VCPU=1)
    if(NOT GOCOS_WASI)
      target_link_libraries(wasmgocos PUBLIC WinHvPlatform)
    else()
      target_sources(wasmgocos PRIVATE src/vcpu.cc)
    endif()
    # JS: WASMv8bindings is the only FetchContent of quickjs-ng (target qjs,
    # v0.16.2). wasmv8_facade is that package. Do not declare quickjs here.
    set(WV8_DIR "${_gocos_sib}/WASMv8bindings")
    if(NOT TARGET wasmv8_facade)
      if(TARGET wasmv8_cppgc OR TARGET qjs)
        message(FATAL_ERROR
          "wasmv8_cppgc or qjs is already defined without wasmv8_facade. "
          "Add WASMv8bindings once, as wasmv8_facade.")
      endif()
      if(NOT EXISTS "${WV8_DIR}/CMakeLists.txt")
        message(FATAL_ERROR
          "WASMGocOS requires ${WV8_DIR} (wasmv8_facade). "
          "Do not FetchContent quickjs-ng in this repo.")
      endif()
      add_subdirectory("${WV8_DIR}" "${CMAKE_BINARY_DIR}/wasmv8" EXCLUDE_FROM_ALL)
    endif()
    if(NOT TARGET wasmv8_facade)
      message(FATAL_ERROR "WASMv8bindings did not define wasmv8_facade")
    endif()
    target_link_libraries(wasmgocos PUBLIC wasmv8_facade)
  endif()
  # Kernel core communication. Not add_subdirectory: that repo registers
  # its own ctest binaries. The guest OpenSSL memory BIO in the go++
  # sysroot reads and writes this pipe.
  set(WNS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../WASMNetStack")
  if(NOT TARGET wns AND EXISTS "${WNS_DIR}/include/wns/stack.h")
    add_library(wns STATIC
      "${WNS_DIR}/src/addr.cc"
      "${WNS_DIR}/src/locator.cc"
      "${WNS_DIR}/src/classify.cc"
      "${WNS_DIR}/src/nat.cc"
      "${WNS_DIR}/src/gossip.cc"
      "${WNS_DIR}/src/wss.cc"
      "${WNS_DIR}/src/rpc.cc"
      "${WNS_DIR}/src/chunk.cc"
      "${WNS_DIR}/src/demux.cc"
      "${WNS_DIR}/src/assoc.cc"
      "${WNS_DIR}/src/gls.cc"
      "${WNS_DIR}/src/bgp.cc"
      "${WNS_DIR}/src/dns.cc"
      "${WNS_DIR}/src/ua.cc"
      "${WNS_DIR}/src/anycast.cc"
      "${WNS_DIR}/src/stack.cc"
      "${WNS_DIR}/src/ipify.cc"
      "${WNS_DIR}/src/flow.cc"
      "${WNS_DIR}/src/proxy.cc"
      "${WNS_DIR}/src/sock.cc"
      "${WNS_DIR}/src/link.cc"
      "${WNS_DIR}/src/proxyd.cc"
      "${WNS_DIR}/src/tty.cc"
      "${WNS_DIR}/src/status.cc"
      "${WNS_DIR}/src/c/system.cc"
    )
    target_include_directories(wns
      PUBLIC "${WNS_DIR}/include"
      PRIVATE "${WNS_DIR}/src"
    )
    if(NOT GOCOS_WASI)
      target_link_libraries(wns PUBLIC ws2_32)
    endif()
  endif()
  if(TARGET wns AND TARGET wasmturbo_engine)
    target_sources(wasmgocos PRIVATE src/net.cc)
    target_link_libraries(wasmgocos PUBLIC wns)
    target_compile_definitions(wasmgocos PUBLIC WASMGOCOS_HAS_NET=1)
  endif()
endif()

# The catalogs wasigocvm.bat compiles into every module. With the kernel
# runtime they are already TurboSpace's (wasmwin32, and the nix catalog in
# wasmturbo_engine); a second copy here would define them twice.
if(GOCOS_WASI)
  if(NOT TARGET wasmwin32)
    target_sources(wasmgocos PRIVATE
      "${_gocos_sib}/WASMWin32/src/catalog.cc"
      "${_gocos_sib}/WASMWin32/src/host_wasi.cc")
  endif()
  if(NOT TARGET wasmturbo_engine)
    target_sources(wasmgocos PRIVATE "${_gocos_sib}/WASMnix/src/catalog.cc")
  endif()
endif()

if(MSVC)
  target_compile_options(wasmgocos PRIVATE /W4)
else()
  target_compile_options(wasmgocos PRIVATE -Wall -Wextra)
endif()

enable_testing()

add_executable(boot_test tests/boot_test.cc)
target_link_libraries(boot_test PRIVATE wasmgocos)
add_test(NAME boot_test COMMAND boot_test)

add_executable(krnl_test tests/krnl_test.cc)
target_link_libraries(krnl_test PRIVATE wasmgocos)
add_test(NAME krnl_test COMMAND krnl_test)

add_executable(jit_test tests/jit_test.cc)
target_link_libraries(jit_test PRIVATE wasmgocos)
add_test(NAME jit_test COMMAND jit_test)

add_executable(nix_test tests/nix_test.cc)
target_link_libraries(nix_test PRIVATE wasmgocos)
add_test(NAME nix_test COMMAND nix_test)

add_executable(sys_test tests/sys_test.cc)
target_link_libraries(sys_test PRIVATE wasmgocos)
add_test(NAME sys_test COMMAND sys_test)

add_executable(shell_test tests/shell_test.cc)
target_link_libraries(shell_test PRIVATE wasmgocos)
add_test(NAME shell_test COMMAND shell_test)

add_executable(console_test tests/console_test.cc)
target_link_libraries(console_test PRIVATE wasmgocos)
add_test(NAME console_test COMMAND console_test)

add_executable(unil_test tests/unil_test.cc)
target_link_libraries(unil_test PRIVATE wasmgocos)
target_compile_definitions(unil_test PRIVATE GOCOS_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
add_test(NAME unil_test COMMAND unil_test)

add_executable(dll_test tests/dll_test.cc)
target_link_libraries(dll_test PRIVATE wasmgocos)
add_test(NAME dll_test COMMAND dll_test)
# Persistence past the process: one boot saves a container, the next
# process opens it.
add_test(NAME unil_save COMMAND unil_test save "${CMAKE_CURRENT_BINARY_DIR}/persist.unil")
add_test(NAME unil_reboot COMMAND unil_test reboot "${CMAKE_CURRENT_BINARY_DIR}/persist.unil")
set_tests_properties(unil_save PROPERTIES FIXTURES_SETUP gocos_persist)
set_tests_properties(unil_reboot PROPERTIES FIXTURES_REQUIRED gocos_persist)

# A wasigocvm guest run as a container service. The guest is built by
# ~/go++/wasigocvm.bat from examples/service/main.go; with no toolchain the
# test still runs and fails on the missing module rather than skipping.
set(GOCOS_GUEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/examples/service")
set(GOCOS_GUEST_WASM "${GOCOS_GUEST_DIR}/service.wasm")
set(GOCOS_WASIGOCVM "${CMAKE_CURRENT_SOURCE_DIR}/../go++/wasigocvm.bat")
add_executable(guest_test tests/guest_test.cc)
target_link_libraries(guest_test PRIVATE wasmgocos)
# A C guest on wasi:filesystem, built by go++'s own wasip2 clang.
set(GOCOS_FS_WASM "${CMAKE_CURRENT_BINARY_DIR}/fsguest.wasm")
set(GOCOS_WASIP2_CLANG "${CMAKE_CURRENT_SOURCE_DIR}/../go++/toolchain/bin/wasm32-wasip2-clang.exe")
target_compile_definitions(guest_test PRIVATE GOCOS_GUEST_WASM="${GOCOS_GUEST_WASM}"
  GOCOS_FS_WASM="${GOCOS_FS_WASM}"
  GOCOS_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
if(EXISTS "${GOCOS_WASIP2_CLANG}")
  add_custom_command(OUTPUT "${GOCOS_FS_WASM}"
    COMMAND "${GOCOS_WASIP2_CLANG}" -O2 -fuse-ld=lld
      "--sysroot=${CMAKE_CURRENT_SOURCE_DIR}/../go++/toolchain/sysroot"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/fsguest.c"
      -o "${GOCOS_FS_WASM}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/fsguest.c"
    COMMENT "wasm32-wasip2-clang tests/data/fsguest.c"
    VERBATIM)
  add_custom_target(fsguest_wasm ALL DEPENDS "${GOCOS_FS_WASM}")
  add_dependencies(guest_test fsguest_wasm)
endif()
if(WIN32 AND EXISTS "${GOCOS_WASIGOCVM}")
  file(TO_NATIVE_PATH "${GOCOS_WASIGOCVM}" _gocos_wasigocvm_native)
  file(TO_NATIVE_PATH "${GOCOS_GUEST_DIR}/main.go" _gocos_guest_go_native)
  file(TO_NATIVE_PATH "${GOCOS_GUEST_WASM}" _gocos_guest_wasm_native)
  add_custom_command(OUTPUT "${GOCOS_GUEST_WASM}"
    COMMAND cmd /c "${_gocos_wasigocvm_native}" "${_gocos_guest_go_native}" -o "${_gocos_guest_wasm_native}"
    DEPENDS "${GOCOS_GUEST_DIR}/main.go"
    COMMENT "wasigocvm.bat examples/service/main.go"
    VERBATIM)
  add_custom_target(service_wasm ALL DEPENDS "${GOCOS_GUEST_WASM}")
  add_dependencies(guest_test service_wasm)
endif()
add_test(NAME guest_test COMMAND guest_test)

add_executable(gocos_example examples/hello/main.cc)
target_link_libraries(gocos_example PRIVATE wasmgocos)

add_executable(plan9 examples/plan9/main.cc)
target_link_libraries(plan9 PRIVATE wasmgocos)

add_custom_target(wasmgocos_everything DEPENDS
  wasmgocos boot_test krnl_test jit_test nix_test sys_test shell_test unil_test guest_test gocos_example
)

foreach(_gocos_module boot_test krnl_test jit_test nix_test sys_test shell_test unil_test
    guest_test gocos_example plan9)
  gocos_wasi_link(${_gocos_module})
endforeach()
