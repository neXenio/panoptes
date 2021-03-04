add_library(Catch INTERFACE)
target_include_directories(Catch INTERFACE ../external/catch)

set (UNIT_TEST_NAME "PanoptesUnitTests")

include(ProcessorCount)
include(RegisterCatchTests)

message (STATUS "Running CMake for ${PROJECT_NAME} ${DVAULT_VERSION_STRING} testing binaries")

set (PANOPTES_TESTHELPER_SOURCES
  "testutil/CoreFoundationHelper.h"
  "testutil/test_helper.h"
  "testutil/FileSandbox.h"
)

set (PANOPTES_GLOBAL_TEST_SOURCES
  "main.cpp"
  "catch_wrapper.h"
)

set (PANOPTES_TEST_SOURCES
  "unit/u_FileWatcher.cpp"
)

#
# # # tests
#
add_executable (${UNIT_TEST_NAME} ${PANOPTES_TEST_SOURCES}
                                  ${PANOPTES_GLOBAL_TEST_SOURCES}
                                  ${PANOPTES_TESTHELPER_SOURCES})
set_target_properties(${UNIT_TEST_NAME} PROPERTIES CXX_STANDARD 17)
target_include_directories(${UNIT_TEST_NAME}
                           PUBLIC
                               ${PANOPTES_TESTING}
                               ${CMAKE_CURRENT_BINARY_DIR}
                           PRIVATE
                               $<TARGET_PROPERTY:${PANOPTES_LIBRARY_NAME},INTERFACE_INCLUDE_DIRECTORIES>)
target_link_libraries(${UNIT_TEST_NAME}
                         PUBLIC   Catch
                                  ${CMAKE_THREAD_LIBS_INIT}
                                  ${PANOPTES_LIBRARY_NAME})

if (APPLE)
  target_link_libraries(${UNIT_TEST_NAME}
                          PUBLIC    ${CORE_SERVICES_LIBRARY})
endif (APPLE)

register_catch_tests(${UNIT_TEST_NAME} ${PANOPTES_TEST_SOURCES})
add_custom_target(tests DEPENDS ${UNIT_TEST_NAME})
add_custom_target(check
                  COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --no-label-summary --build-config $<CONFIG>
                  DEPENDS tests)

ProcessorCount(CPU_CORES)
add_custom_target(parallel_check
                  COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --parallel ${CPU_CORES} --no-label-summary --build-config $<CONFIG>
                  DEPENDS tests)

foreach(TEST_LABEL ${CTEST_REGISTERED_LABELS})
  string(REPLACE " " "_" TEST_LABEL_MANGLED ${TEST_LABEL})
  add_custom_target(check_${TEST_LABEL_MANGLED}
                    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -L "${TEST_LABEL}" --build-config $<CONFIG>
                    DEPENDS tests)
endforeach(TEST_LABEL ${CTEST_REGISTERED_LABELS})

if (WITH_CPPCHECK)
    add_cppcheck (${UNIT_TEST_NAME})
endif (WITH_CPPCHECK)
