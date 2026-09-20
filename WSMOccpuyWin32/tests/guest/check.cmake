# Run the occupancy guest and match tests/golden/expected/occupy.txt
# verbatim. Do not rewrite guest output.
if(NOT DEFINED GUEST)
  message(FATAL_ERROR "GUEST is required")
endif()
if(NOT DEFINED EXPECTED)
  message(FATAL_ERROR "EXPECTED is required")
endif()

execute_process(
  COMMAND "${GUEST}"
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err
)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "guest exit ${rc}\n${out}${err}")
endif()
file(READ "${EXPECTED}" want)
if(NOT out STREQUAL want)
  message(FATAL_ERROR "guest output mismatch\nwant:\n${want}\ngot:\n${out}")
endif()
