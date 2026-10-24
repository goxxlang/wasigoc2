cmake_minimum_required(VERSION 3.16)
project(WASMNetStack LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(wns
  src/addr.cc
  src/locator.cc
  src/classify.cc
  src/nat.cc
  src/gossip.cc
  src/wss.cc
  src/rpc.cc
  src/chunk.cc
  src/demux.cc
  src/assoc.cc
  src/gls.cc
  src/bgp.cc
  src/dns.cc
  src/ua.cc
  src/anycast.cc
  src/stack.cc
  src/ipify.cc
  src/flow.cc
  src/proxy.cc
  src/tty.cc
  src/sock.cc
  src/link.cc
  src/proxyd.cc
  src/status.cc
  src/c/system.cc
)

target_include_directories(wns
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(MSVC)
  target_compile_options(wns PRIVATE /W4)
else()
  target_compile_options(wns PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
  target_link_libraries(wns PUBLIC ws2_32)
endif()

enable_testing()

add_executable(wns_tests
  tests/test_main.cc
  tests/addr/parse.cc
  tests/gossip/ip_change.cc
  tests/session/wss_rpc.cc
  tests/classify/vpn_tor.cc
  tests/classify/real_internet.cc
  tests/chunk/roundtrip.cc
  tests/chunk/init.cc
  tests/demux/stream0.cc
  tests/gls/swap_pop.cc
  tests/bgp/session.cc
  tests/dns/wire.cc
  tests/ua/route.cc
  tests/anycast/p2p.cc
  tests/rpc/occupancy.cc
  tests/fabric/loopback.cc
  tests/ipify/json.cc
  tests/ipify/listen.cc
  tests/flow/sack.cc
  tests/flow/reliability.cc
  tests/proxy/wire.cc
  tests/proxy/mux.cc
  tests/proxy/loopback.cc
  tests/tty/wire.cc
)
target_include_directories(wns_tests PRIVATE tests)
target_link_libraries(wns_tests PRIVATE wns)
add_test(NAME wns_tests COMMAND wns_tests)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "WASI")
  add_executable(wns_ipify tools/ipify/ipify.cc)
  target_link_libraries(wns_ipify PRIVATE wns)
  add_executable(wns_proxy tools/proxy/proxy.cc)
  target_link_libraries(wns_proxy PRIVATE wns)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "WASI")
  set(CMAKE_EXECUTABLE_SUFFIX ".wasm")
  add_executable(wns_wasm src/c/wasi_start.cc)
  target_link_libraries(wns_wasm PRIVATE "-Wl,--whole-archive" wns "-Wl,--no-whole-archive")
  target_link_options(wns_wasm PRIVATE "LINKER:--export-all" "LINKER:--no-entry" "LINKER:--allow-undefined")
  set_target_properties(wns_wasm PROPERTIES OUTPUT_NAME "wns")
endif()
