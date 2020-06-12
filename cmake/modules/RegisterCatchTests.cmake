# - Register all tests defined in a Catch-based unit test set
#
#  Copyright (c) 2016 René Meusel <rene.meusel@nexenio.com>
#

set(CTEST_REGISTERED_LABELS)

set(JUNIT_TEST_LOG_DIR "${CMAKE_BINARY_DIR}/testlogs")

#
# _register_catch_test(<test target> <test name> <test labels>)
#
# This is an internal function to be called by register_catch_tests().
# It prepares the Catch test name and labels for CTest and registers
# a single given test case with CTest.
#
#   Paramters:
#      _target   the test runner target
#                usually created via add_executable()
#      _retries  the number of retries to perform in case the test fails
#      _name     the name as used inside Catch
#      _labels   the label string as used inside Catch "[label1][label2]"
#
FUNCTION(_REGISTER_CATCH_TEST _target _retries _name _labels)

  # CTest does not allow special characters and spaces in the test names
  # therefore we need to mangle the Catch test name
  string(REPLACE " " "_" TEST_CASE_MANGLED "${_name}")
  string(REPLACE ":" "_" TEST_CASE_MANGLED "${TEST_CASE_MANGLED}")

  # Parse the Catch test labels into a list of CTest labels
  string(REPLACE "[" ";" TEST_LABELS ${_labels})
  string(REPLACE "]" ";" TEST_LABELS ${TEST_LABELS})
  list(REMOVE_ITEM TEST_LABELS "")
  list(REMOVE_ITEM TEST_LABELS " ")
  list(REMOVE_DUPLICATES TEST_LABELS)

  # update CTEST_REGISTERED_LABELS
  set(CTEST_REGISTERED_LABELS "${CTEST_REGISTERED_LABELS};${TEST_LABELS}" PARENT_SCOPE)

  set(TEST_LOG_OUTPUT "${JUNIT_TEST_LOG_DIR}/${TEST_CASE_MANGLED}.log")

  find_file(TEST_RUNNER_WRAPPER "RunCatchTest.cmake"
            PATHS ${CMAKE_MODULE_PATH}
            DOC "test wrapper script to be used for ctest execution")

  # Register the prepared Catch test with CTest
  add_test(NAME "${TEST_CASE_MANGLED}"
           COMMAND ${CMAKE_COMMAND} -DCATCH_TEST_TARGET_NAME=${_target}
                                    -DCATCH_TEST_EXE=$<TARGET_FILE:${_target}>
                                    -DCATCH_TEST_SUITE_NAME="${_name} - ${CMAKE_SYSTEM}"
                                    -DCATCH_TEST_LOG=${TEST_LOG_OUTPUT}
                                    -DCATCH_TEST_NAME="${_name}"
                                    -DCATCH_TEST_RETRIES=${_retries}
                                    -DCATCH_TEST_TIMEOUT=300  # seconds
                                    -P "${TEST_RUNNER_WRAPPER}")
  set_tests_properties(${TEST_CASE_MANGLED} PROPERTIES LABELS "${TEST_LABELS}")

  message(STATUS "Found and registered test case: \"${_name}\" with labels ${TEST_LABELS}")

ENDFUNCTION()

#
# _register_catch_tests(<test target> <retry flag> <source file list>)
#
#   Paramters:
#      _target   the test runner target
#                usually created via add_executable()
#      _retries  the number of retries to perform in case the test fails
#      _n...     a list of a C++ source files containing Catch tests
#
FUNCTION(_REGISTER_CATCH_TESTS _target _retries)

  set(CATCH_TEST_CASES "")

  # read through all catch test source files searching for
  # either the TEST_CASE or the SCENARIO macro and collect
  # the containe test case names (first string parameter)
  foreach (source ${ARGN})
    file(READ "${source}" contents)
    STRING(REGEX REPLACE ";" "\\\\;" contents "${contents}")
    STRING(REGEX REPLACE "\n" ";" contents "${contents}")

    foreach(line ${contents})
      string(REGEX MATCHALL "^[ ]*(TEST_CASE|SCENARIO)[ ]*\\([ ]*\"([^\"]*)\"[ ]*,[ ]*\"([^\"]*)\"" TEST_CASE "${line}")
      if(TEST_CASE)
        set(TEST_TYPE "${CMAKE_MATCH_1}")
        set(TEST_CASE "${CMAKE_MATCH_2}")
        set(TEST_LABELS "${CMAKE_MATCH_3}")
        if(TEST_TYPE STREQUAL "SCENARIO")
          set(TEST_CASE "Scenario: ${TEST_CASE}")
        endif()
        _REGISTER_CATCH_TEST(${_target} ${_retries} "${TEST_CASE}" ${TEST_LABELS})
      endif()
    endforeach()
  endforeach()

  list(REMOVE_DUPLICATES CTEST_REGISTERED_LABELS)
  set(CTEST_REGISTERED_LABELS ${CTEST_REGISTERED_LABELS} PARENT_SCOPE)

  file(MAKE_DIRECTORY ${JUNIT_TEST_LOG_DIR})

ENDFUNCTION() # _REGISTER_CATCH_TESTS

#
# register_catch_tests(<test target> <source file list>)
#
# This parses the test source files for appearance of Catch's
# TEST_CASE() or SCENARIO() macro and registers those to CTest.
#
# After calling this function CTEST_REGISTERED_LABELS will contain
# all test labels found in the Catch source files
#
# Note: for the test autodiscovery to work properly, the Catch
#       macro invocations must not be split into multiple lines.
#
#   Paramters:
#      _target   the test runner target
#                usually created via add_executable()
#      _n...     a list of a C++ source files containing Catch tests
#
FUNCTION(REGISTER_CATCH_TESTS _target)

    _register_catch_tests(${_target} 0 ${ARGN})

ENDFUNCTION() # REGISTER_CATCH_TESTS

#
# register_catch_tests_with_retry(<test target> <source file list>)
#
# This parses the test source files for appearance of Catch's
# TEST_CASE() or SCENARIO() macro and registers those to CTest.
#
# After calling this function CTEST_REGISTERED_LABELS will contain
# all test labels found in the Catch source files
#
# If the test fails it will be retried once to mitigate flaky tests.
#
# Note: for the test autodiscovery to work properly, the Catch
#       macro invocations must not be split into multiple lines.
#
#   Paramters:
#      _target   the test runner target
#                usually created via add_executable()
#      _n...     a list of a C++ source files containing Catch tests
#
FUNCTION(REGISTER_CATCH_TESTS_WITH_RETRY _target)

    _register_catch_tests(${_target} 3 ${ARGN})

ENDFUNCTION() # REGISTER_CATCH_TESTS
