cmake_minimum_required(VERSION 3.16)
project(WASMNix LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(wasmnix STATIC
  src/catalog.cc
  src/call.cc
)

target_include_directories(wasmnix PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(MSVC)
  target_compile_options(wasmnix PRIVATE /W4)
else()
  target_compile_options(wasmnix PRIVATE -Wall -Wextra)
endif()

enable_testing()

add_executable(wsl_test tests/wsl_test.cc)
target_link_libraries(wsl_test PRIVATE wasmnix)
add_test(NAME wsl_test COMMAND wsl_test)

add_executable(nix_example examples/hello/main.cc)
target_link_libraries(nix_example PRIVATE wasmnix)

add_custom_target(wasmnix_everything DEPENDS wasmnix wsl_test nix_example)
