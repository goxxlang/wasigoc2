cmake_minimum_required(VERSION 3.16)
project(WASMDroid LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Phone Link (YourPhone). One call path.
add_library(wasmdroid STATIC
  src/catalog.cc
  src/call.cc
)

target_include_directories(wasmdroid PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(MSVC)
  target_compile_options(wasmdroid PRIVATE /W4)
else()
  target_compile_options(wasmdroid PRIVATE -Wall -Wextra)
endif()

enable_testing()

add_executable(phonelink_test tests/phonelink_test.cc)
target_link_libraries(phonelink_test PRIVATE wasmdroid)
add_test(NAME phonelink_test COMMAND phonelink_test)

add_executable(droid_example examples/hello/main.cc)
target_link_libraries(droid_example PRIVATE wasmdroid)

add_custom_target(wasmdroid_everything DEPENDS wasmdroid phonelink_test droid_example)
