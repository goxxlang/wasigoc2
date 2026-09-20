cmake_minimum_required(VERSION 3.16)
project(WASMNix LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# wasmnix: a wasigocvm-facing projection of public Linux man-pages
# (syscalls(2) / POSIX.1) plus WSL's Linux-side hop
# (https://github.com/microsoft/WSL — wslpath / wslinfo / interop),
# the Nix CLI, the Linux hypervisor uapi (kvm.h / vfio.h / vhost.h /
# mshv.h), and the vmlinux EXPORT_SYMBOL surface (slab/mm/irq/sched/
# kvm_host — ntoskrnl analog). Native Linux/WSL calls libc and nix.
# Native Windows trampolines through wsl.exe. KVM_CREATE_VM / kmalloc
# stay in-module (the mapper does not JIT x86). wasm32 uses libc.
add_library(wasmnix STATIC
  src/catalog.cc
)
if(WIN32 AND NOT CMAKE_SYSTEM_NAME STREQUAL "WASI")
  target_sources(wasmnix PRIVATE src/host_wsl.cc)
elseif(CMAKE_SYSTEM_NAME STREQUAL "WASI")
  target_sources(wasmnix PRIVATE src/host_wasi.cc)
else()
  target_sources(wasmnix PRIVATE src/host_linux.cc)
endif()

target_include_directories(wasmnix PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(MSVC)
  target_compile_options(wasmnix PRIVATE /W4)
else()
  target_compile_options(wasmnix PRIVATE -Wall -Wextra)
endif()

enable_testing()

add_executable(linux_test tests/linux_test.cc)
target_link_libraries(linux_test PRIVATE wasmnix)
add_test(NAME linux_test COMMAND linux_test)

add_executable(wsl_test tests/wsl_test.cc)
target_link_libraries(wsl_test PRIVATE wasmnix)
add_test(NAME wsl_test COMMAND wsl_test)

add_executable(nix_test tests/nix_test.cc)
target_link_libraries(nix_test PRIVATE wasmnix)
add_test(NAME nix_test COMMAND nix_test)

add_executable(kvm_test tests/kvm_test.cc)
target_link_libraries(kvm_test PRIVATE wasmnix)
add_test(NAME kvm_test COMMAND kvm_test)

add_executable(kernel_test tests/kernel_test.cc)
target_link_libraries(kernel_test PRIVATE wasmnix)
add_test(NAME kernel_test COMMAND kernel_test)

add_executable(nix_example examples/hello/main.cc)
target_link_libraries(nix_example PRIVATE wasmnix)

add_custom_target(wasmnix_everything DEPENDS
  wasmnix linux_test wsl_test nix_test kvm_test kernel_test nix_example
)
