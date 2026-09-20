# wasigocvm (Was2p successor) — own target, not stock wasip2.
# See ~/Go++/docs/wasigocvm.md. Sets WASIGO_GOCVM so occupancy can
# RegisterHostBridge against runtime.hpp without rewriting it.
# Go++ compile.bat / wasigocvm is one VM: occupancy stacks win32.* on
# the in-module machine. It does not exec a second gocvm.exe.
if(NOT DEFINED WASIGO_ROOT)
  if(DEFINED ENV{WASIGO_ROOT})
    set(WASIGO_ROOT "$ENV{WASIGO_ROOT}")
  elseif(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../../Go++/src/runtime.hpp")
    get_filename_component(WASIGO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../Go++" ABSOLUTE)
  elseif(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../../go++/src/runtime.hpp")
    get_filename_component(WASIGO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../go++" ABSOLUTE)
  elseif(DEFINED ENV{USERPROFILE} AND EXISTS "$ENV{USERPROFILE}/Go++/src/runtime.hpp")
    set(WASIGO_ROOT "$ENV{USERPROFILE}/Go++")
  elseif(DEFINED ENV{USERPROFILE} AND EXISTS "$ENV{USERPROFILE}/go++/src/runtime.hpp")
    set(WASIGO_ROOT "$ENV{USERPROFILE}/go++")
  endif()
endif()
set(WASIGO_GOCVM 1)
set(WOW_HAS_GOCVM 1)
