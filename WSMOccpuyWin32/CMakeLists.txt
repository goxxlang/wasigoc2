cmake_minimum_required(VERSION 3.16)
project(WSMOccpuyWin32 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(wow
  src/catalog/catalog.cc
  src/surface/surface.cc
  src/store/store.cc
  src/event/event.cc
  src/occupancy/occupancy.cc
  src/ipc/ipc.cc
  src/harness/harness.cc
  src/gocvm/bridge.cc
  src/c/system.cc
  src/driver/driver.cc
  src/posix/posix.cc
  src/tty/tty.cc
)

target_include_directories(wow
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ~/WASMWin32 — occupancy Calls wasmwin32_call / wasmwin32_catalog as-is.
# This hop owns win32.* topics. It does not rewrite host_win.cc.
set(WOW_WIN32_DIR "")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../WASMWin32/CMakeLists.txt")
  get_filename_component(WOW_WIN32_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../WASMWin32" ABSOLUTE)
elseif(DEFINED ENV{USERPROFILE} AND EXISTS "$ENV{USERPROFILE}/WASMWin32/CMakeLists.txt")
  set(WOW_WIN32_DIR "$ENV{USERPROFILE}/WASMWin32")
endif()
if(WOW_WIN32_DIR AND NOT TARGET wasmwin32)
  add_subdirectory("${WOW_WIN32_DIR}" "${CMAKE_BINARY_DIR}/wasmwin32" EXCLUDE_FROM_ALL)
endif()
if(NOT TARGET wasmwin32)
  message(FATAL_ERROR
    "WASMWin32 sibling checkout required at ../WASMWin32 "
    "(this hop Calls wasmwin32_call, it does not rewrite it)")
endif()
target_link_libraries(wow PUBLIC wasmwin32)
target_compile_definitions(wow PUBLIC WOW_HAS_WIN32=1)
message(STATUS "wow: WASMWin32 hop at ${WOW_WIN32_DIR}")

# ~/Go++ GocVMRuntime — load runtime.hpp, do not rewrite it.
# compile.bat / wasigocvm (Was2p successor) is one VM. Occupancy stacks
# win32.* on that machine. It does not exec a second gocvm.exe.
set(WOW_GOXX "")
if(DEFINED ENV{WASIGO_ROOT} AND EXISTS "$ENV{WASIGO_ROOT}/src/runtime.hpp")
  set(WOW_GOXX "$ENV{WASIGO_ROOT}")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../Go++/src/runtime.hpp")
  get_filename_component(WOW_GOXX "${CMAKE_CURRENT_SOURCE_DIR}/../Go++" ABSOLUTE)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../go++/src/runtime.hpp")
  get_filename_component(WOW_GOXX "${CMAKE_CURRENT_SOURCE_DIR}/../go++" ABSOLUTE)
elseif(DEFINED ENV{USERPROFILE} AND EXISTS "$ENV{USERPROFILE}/Go++/src/runtime.hpp")
  set(WOW_GOXX "$ENV{USERPROFILE}/Go++")
elseif(DEFINED ENV{USERPROFILE} AND EXISTS "$ENV{USERPROFILE}/go++/src/runtime.hpp")
  set(WOW_GOXX "$ENV{USERPROFILE}/go++")
endif()
find_package(Threads REQUIRED)
target_link_libraries(wow PUBLIC Threads::Threads)

if(WOW_GOXX)
  target_include_directories(wow PUBLIC "${WOW_GOXX}/src")
  target_compile_definitions(wow PUBLIC WOW_HAS_GOCVM=1)
  message(STATUS "wow: GocVMRuntime at ${WOW_GOXX}")
else()
  message(STATUS "wow: Go++ runtime.hpp not found — occupancy bridge still Call()s")
endif()

if(MSVC)
  target_compile_options(wow PRIVATE /W4)
else()
  target_compile_options(wow PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()

add_executable(wow_tests
  tests/test_main.cc
  tests/catalog/lookup.cc
  tests/surface/kinds.cc
  tests/store/table.cc
  tests/occupancy/api.cc
  tests/harness/fire.cc
  tests/gocvm/bridge.cc
  tests/ipc/invoke.cc
  tests/driver/info.cc
)
target_include_directories(wow_tests PRIVATE tests)
target_link_libraries(wow_tests PRIVATE wow)
add_test(NAME wow_tests COMMAND wow_tests)
if(TARGET win32_test)
  add_dependencies(wow_tests win32_test)
endif()
if(TARGET wsl_test)
  add_dependencies(wow_tests wsl_test)
endif()

add_executable(wow_occupy examples/occupy/main.cc)
target_link_libraries(wow_occupy PRIVATE wow)

# Occupancy guest: Go++ compile.bat / wasigocvm is one VM. wasigoc
# transpiles occupy.go; occupancy HostBridge stacks win32.* on that
# machine. Do not rewrite the guest as a host applet or exec a second
# gocvm.exe.
set(WASIGOC_EXE "")
if(WOW_GOXX)
  if(EXISTS "${WOW_GOXX}/build/wasigoc.exe")
    set(WASIGOC_EXE "${WOW_GOXX}/build/wasigoc.exe")
  elseif(EXISTS "${WOW_GOXX}/build/wasigoc")
    set(WASIGOC_EXE "${WOW_GOXX}/build/wasigoc")
  elseif(EXISTS "${WOW_GOXX}/build-fork/wasigoc.exe")
    set(WASIGOC_EXE "${WOW_GOXX}/build-fork/wasigoc.exe")
  endif()
endif()
set(WASIGOCVM_BAT "")
if(WOW_GOXX)
  if(EXISTS "${WOW_GOXX}/compile.bat")
    set(WASIGOCVM_BAT "${WOW_GOXX}/compile.bat")
  elseif(EXISTS "${WOW_GOXX}/wasigocvm.bat")
    set(WASIGOCVM_BAT "${WOW_GOXX}/wasigocvm.bat")
  endif()
endif()
if(WASIGOC_EXE AND WOW_GOXX)
  set(WOW_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
  set(WOW_GUEST_CPP "${WOW_GEN_DIR}/occupy_gen.cpp")
  file(MAKE_DIRECTORY "${WOW_GEN_DIR}")
  add_custom_command(
    OUTPUT "${WOW_GUEST_CPP}"
    COMMAND "${WASIGOC_EXE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/gocvm/occupy.go"
            -o "${WOW_GUEST_CPP}"
            --out-dir "${WOW_GEN_DIR}"
    DEPENDS
      "${CMAKE_CURRENT_SOURCE_DIR}/examples/gocvm/occupy.go"
      "${WOW_GOXX}/src/runtime.hpp"
    COMMENT "wasigoc: occupancy guest occupy.go"
    VERBATIM
  )
  add_executable(wow_guest
    "${WOW_GUEST_CPP}"
    src/gocvm/install.cc
  )
  target_include_directories(wow_guest PRIVATE "${WOW_GEN_DIR}")
  target_link_libraries(wow_guest PRIVATE wow)
  target_compile_definitions(wow_guest PRIVATE WASIGO_GOCVM_BRIDGE=1)
  if(NOT MSVC)
    set_source_files_properties("${WOW_GUEST_CPP}" PROPERTIES
      COMPILE_FLAGS "-Wno-unused-parameter")
  endif()
  if(MSVC)
    target_compile_options(wow_guest PRIVATE /W4)
  else()
    target_compile_options(wow_guest PRIVATE -Wall -Wextra -Wpedantic)
  endif()
  add_test(
    NAME wow_guest
    COMMAND ${CMAKE_COMMAND}
            "-DGUEST=$<TARGET_FILE:wow_guest>"
            "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/tests/golden/expected/occupy.txt"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/guest/check.cmake"
  )
  message(STATUS "wow: occupancy guest via ${WASIGOC_EXE}")
  if(WASIGOCVM_BAT)
    set(WOW_GUEST_WASM "${CMAKE_CURRENT_BINARY_DIR}/occupy.wasigocvm.wasm")
    add_custom_command(
      OUTPUT "${WOW_GUEST_WASM}"
      COMMAND "${WASIGOCVM_BAT}"
              "${CMAKE_CURRENT_SOURCE_DIR}/examples/gocvm/occupy.go"
              -o "${WOW_GUEST_WASM}"
      DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/gocvm/occupy.go"
        "${WOW_GOXX}/src/runtime.hpp"
      WORKING_DIRECTORY "${WOW_GOXX}"
      COMMENT "compile.bat / wasigocvm: occupancy guest occupy.go (one VM)"
      VERBATIM
    )
    add_custom_target(wow_guest_wasigocvm DEPENDS "${WOW_GUEST_WASM}")
    message(STATUS "wow: wasigocvm guest via ${WASIGOCVM_BAT}")
  endif()
else()
  message(STATUS "wow: wasigoc.exe not found — occupancy guest skipped")
endif()
