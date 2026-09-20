cmake_minimum_required(VERSION 3.16)
project(WASMPELoader LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Header-only PE mapper for wasigocvm / WASMWin32 LoadLibrary.
# Maps MZ/PE. MainDLL / TLS run on the WASMWin32 LoadLibrary hop.
add_library(wasmpe INTERFACE)
target_include_directories(wasmpe INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra)
endif()

enable_testing()

add_executable(wasmpe_test tests/loader_test.cc)
target_link_libraries(wasmpe_test PRIVATE wasmpe)
add_test(NAME wasmpe_test COMMAND wasmpe_test)
