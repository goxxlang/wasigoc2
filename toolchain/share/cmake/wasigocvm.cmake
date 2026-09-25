# wasigocvm CMake toolchain: the wasm32-wasip2 base from wasi-sdk-p2.cmake,
# plus the exception-enabled libc++ sysroot (`toolchain/sysroot/.../eh/`)
# and link set that wasigocvm.bat wires up for its own go++ driver
# (-fwasm-exceptions, -nostdinc++ + EH_INC, -lc++/-lc++abi/-lunwind from
# EH_LIB). CMake projects (WASMLime and its siblings) need the same ABI on
# every translation unit they link together -- not just the one file that
# happens to throw -- since they all share one libc++abi instance.
#
# Usage: cmake -B build-wasigocvm -DCMAKE_TOOLCHAIN_FILE=.../wasigocvm.cmake -DWASIGO_GOCVM=1

include("${CMAKE_CURRENT_LIST_DIR}/wasi-sdk-p2.cmake")

if(NOT WASIGO_SYSROOT)
  if(DEFINED ENV{WASIGO_TOOLCHAIN} AND EXISTS "$ENV{WASIGO_TOOLCHAIN}/sysroot/include")
    set(WASIGO_SYSROOT "$ENV{WASIGO_TOOLCHAIN}/sysroot")
  else()
    set(WASIGO_SYSROOT "${WASI_SDK_PREFIX}/sysroot")
  endif()
endif()

# Same triple probe as wasigocvm.bat's :pick_triple -- prefer a real
# wasm32-wasigocvm eh layout once one exists, fall back to wasip2's.
set(WASIGO_TRIPLE)
if(EXISTS "${WASIGO_SYSROOT}/include/wasm32-wasigocvm/eh/c++/v1/iostream")
  set(WASIGO_TRIPLE "wasm32-wasigocvm")
elseif(EXISTS "${WASIGO_SYSROOT}/include/wasm32-wasip2/eh/c++/v1/iostream")
  set(WASIGO_TRIPLE "wasm32-wasip2")
endif()
if(NOT WASIGO_TRIPLE)
  message(FATAL_ERROR
    "wasigocvm.cmake: no eh/ libc++ under ${WASIGO_SYSROOT}/include -- run "
    "toolchain/bootstrap.sh/.ps1, or set WASIGO_TOOLCHAIN to one that has it.")
endif()

set(WASIGO_EH_INC "${WASIGO_SYSROOT}/include/${WASIGO_TRIPLE}/eh/c++/v1")
set(WASIGO_EH_LIB "${WASIGO_SYSROOT}/lib/${WASIGO_TRIPLE}/eh")

set(CMAKE_CXX_FLAGS_INIT
  "${CMAKE_CXX_FLAGS_INIT} -fexceptions -frtti -fwasm-exceptions -mllvm -wasm-use-legacy-eh=false -nostdinc++ -isystem \"${WASIGO_EH_INC}\" -isystem \"${WASIGO_SYSROOT}/include/${WASIGO_TRIPLE}\" -isystem \"${WASIGO_SYSROOT}/include\"")

set(WASIGO_LINK_FLAGS
  "-L \"${WASIGO_EH_LIB}\" -lc++ -lc++abi -lunwind -Wl,--export=__indirect_function_table")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT} ${WASIGO_LINK_FLAGS}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${CMAKE_SHARED_LINKER_FLAGS_INIT} ${WASIGO_LINK_FLAGS}")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "${CMAKE_MODULE_LINKER_FLAGS_INIT} ${WASIGO_LINK_FLAGS}")
