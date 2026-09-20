cmake_minimum_required(VERSION 3.16)
project(WASMDroid LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# wasmdroid: wasigocvm projection of AOSP Bionic / Binder / Android
# kernel uapi plus the hypervisor hop (kvm.h, pKVM, Gunyah, AVF).
# Native Linux uses Bionic-shaped libc. Native Windows is in-module
# (optional adb). KVM_CREATE_VM / BINDER_WRITE_READ / kmalloc stay
# in-module. The mapper does not JIT x86.
add_library(wasmdroid STATIC
  src/catalog.cc
)
if(WIN32 AND NOT CMAKE_SYSTEM_NAME STREQUAL "WASI")
  target_sources(wasmdroid PRIVATE src/host_win.cc)
elseif(CMAKE_SYSTEM_NAME STREQUAL "WASI")
  target_sources(wasmdroid PRIVATE src/host_wasi.cc)
else()
  target_sources(wasmdroid PRIVATE src/host_linux.cc)
endif()

target_include_directories(wasmdroid PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(MSVC)
  target_compile_options(wasmdroid PRIVATE /W4)
else()
  target_compile_options(wasmdroid PRIVATE -Wall -Wextra)
endif()

enable_testing()

add_executable(bionic_test tests/bionic_test.cc)
target_link_libraries(bionic_test PRIVATE wasmdroid)
add_test(NAME bionic_test COMMAND bionic_test)

add_executable(binder_test tests/binder_test.cc)
target_link_libraries(binder_test PRIVATE wasmdroid)
add_test(NAME binder_test COMMAND binder_test)

add_executable(kvm_test tests/kvm_test.cc)
target_link_libraries(kvm_test PRIVATE wasmdroid)
add_test(NAME kvm_test COMMAND kvm_test)

add_executable(kernel_test tests/kernel_test.cc)
target_link_libraries(kernel_test PRIVATE wasmdroid)
add_test(NAME kernel_test COMMAND kernel_test)

add_executable(adb_test tests/adb_test.cc)
target_link_libraries(adb_test PRIVATE wasmdroid)
add_test(NAME adb_test COMMAND adb_test)

add_executable(droid_example examples/hello/main.cc)
target_link_libraries(droid_example PRIVATE wasmdroid)

add_custom_target(wasmdroid_everything DEPENDS
  wasmdroid bionic_test binder_test kvm_test kernel_test adb_test droid_example
)
