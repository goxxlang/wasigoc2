# Builds one wasigocvm golden through the product driver (wasigocvm.bat on
# Windows, wasigocvm.sh elsewhere), so the golden links exactly what a user's
# `compile.bat prog.go` links, then runs it on this repo's runtime
# (wasitime) and compares its stdout.
#
# Required: DRIVER, RUNTIME, GO_FILE, WASM_FILE
# Optional: EXPECTED_OUTPUT ("\n" stands for a newline) or
#           EXPECTED_OUTPUT_FILE (compared byte for byte)

if(NOT DEFINED DRIVER OR NOT DEFINED RUNTIME OR NOT DEFINED GO_FILE OR NOT DEFINED WASM_FILE)
  message(FATAL_ERROR "run_wasigocvm.cmake: DRIVER, RUNTIME, GO_FILE, and WASM_FILE are required")
endif()

function(_shell script out_cmd)
  if(script MATCHES "\\.bat$")
    set(${out_cmd} cmd /c "${script}" PARENT_SCOPE)
  else()
    set(${out_cmd} bash "${script}" PARENT_SCOPE)
  endif()
endfunction()

_shell("${DRIVER}" _build)
file(REMOVE "${WASM_FILE}")
execute_process(
  COMMAND ${_build} "${GO_FILE}" -o "${WASM_FILE}"
  OUTPUT_VARIABLE build_output
  ERROR_VARIABLE build_error
  RESULT_VARIABLE build_result
)
if(NOT build_result EQUAL 0 OR NOT EXISTS "${WASM_FILE}")
  message(FATAL_ERROR "${DRIVER} failed for ${GO_FILE}:\n${build_output}\n${build_error}")
endif()

_shell("${RUNTIME}" _run)
execute_process(
  COMMAND ${_run} run "${WASM_FILE}"
  OUTPUT_VARIABLE actual_output
  ERROR_VARIABLE run_error
  RESULT_VARIABLE run_result
  TIMEOUT 120
)
if(NOT run_result EQUAL 0)
  message(FATAL_ERROR "${WASM_FILE} exited with ${run_result} under ${RUNTIME}:\n${actual_output}\n${run_error}")
endif()
string(REPLACE "\r\n" "\n" actual_output "${actual_output}")
if(DEFINED EXPECTED_OUTPUT_FILE)
  file(READ "${EXPECTED_OUTPUT_FILE}" expected)
elseif(DEFINED EXPECTED_OUTPUT)
  string(REPLACE "\\n" "\n" expected "${EXPECTED_OUTPUT}")
else()
  return()
endif()
if(NOT actual_output STREQUAL expected)
  message(FATAL_ERROR "output mismatch for ${WASM_FILE}\n--- expected ---\n${expected}\n--- actual ---\n${actual_output}")
endif()
