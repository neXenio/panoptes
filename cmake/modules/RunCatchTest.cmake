if (NOT DEFINED CATCH_TEST_TARGET_NAME)
    message(FATAL_ERROR "missing CATCH_TEST_TARGET_NAME")
endif (NOT DEFINED CATCH_TEST_TARGET_NAME)
if (NOT DEFINED CATCH_TEST_EXE)
    message(FATAL_ERROR "missing CATCH_TEST_EXE")
endif (NOT DEFINED CATCH_TEST_EXE)
if (NOT DEFINED CATCH_TEST_SUITE_NAME)
    message(FATAL_ERROR "missing CATCH_TEST_SUITE_NAME")
endif (NOT DEFINED CATCH_TEST_SUITE_NAME)
if (NOT DEFINED CATCH_TEST_LOG)
    message(FATAL_ERROR "missing CATCH_TEST_LOG")
endif (NOT DEFINED CATCH_TEST_LOG)
if (NOT DEFINED CATCH_TEST_NAME)
    message(FATAL_ERROR "missing CATCH_TEST_NAME")
endif (NOT DEFINED CATCH_TEST_NAME)
if (NOT DEFINED CATCH_TEST_RETRIES)
    message(FATAL_ERROR "missing CATCH_TEST_RETRIES")
endif (NOT DEFINED CATCH_TEST_RETRIES)
if (NOT DEFINED CATCH_TEST_TIMEOUT)
    message(FATAL_ERROR "missing CATCH_TEST_TIMEOUT")
endif (NOT DEFINED CATCH_TEST_TIMEOUT)

# uses the shell utility of choice to slurp the content of a provided file
# onto stdout. As far as I can see there is no proper way to do that with CMake.
FUNCTION(dumpToStdout _target)
    if (WIN32)
        execute_process(COMMAND "powershell.exe" "Get-Content ${_target}")
    else (WIN32)
        execute_process(COMMAND "cat" "${_target}")
    endif(WIN32)
ENDFUNCTION()

while (CATCH_TEST_RETRIES GREATER_EQUAL 0)
    message (STATUS "Run (${CATCH_TEST_RETRIES}): ${CATCH_TEST_EXE} --reporter=console --name=${CATCH_TEST_SUITE_NAME} ${CATCH_TEST_NAME}")
    execute_process(COMMAND
                        "${CATCH_TEST_EXE}"
                        "--reporter=console"
                        "--name=${CATCH_TEST_SUITE_NAME}"
                        "${CATCH_TEST_NAME}"
                    RESULT_VARIABLE
                        RESULT_CODE
                    OUTPUT_FILE
                        ${CATCH_TEST_LOG}
                    ERROR_FILE
                        ${CATCH_TEST_LOG}
                    TIMEOUT
                        ${CATCH_TEST_TIMEOUT})
    if (RESULT_CODE EQUAL 0)
        message(STATUS "Success")
        return()
    else (RESULT_CODE EQUAL 0)
        math(EXPR CATCH_TEST_RETRIES "${CATCH_TEST_RETRIES}-1")
        dumpToStdout(${CATCH_TEST_LOG})
    endif (RESULT_CODE EQUAL 0)
endwhile (CATCH_TEST_RETRIES GREATER_EQUAL 0)

message(FATAL_ERROR "Error: ${RESULT_CODE}")
