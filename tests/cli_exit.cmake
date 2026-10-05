# Usage: cmake -DCMD=<exe> -DARGS=<a;b;c> -DEXPECT=<code> -P cli_exit.cmake
execute_process(COMMAND ${CMD} ${ARGS} RESULT_VARIABLE rc OUTPUT_QUIET ERROR_QUIET)
if(NOT rc EQUAL EXPECT)
    message(FATAL_ERROR "expected exit code ${EXPECT}, got ${rc}")
endif()
