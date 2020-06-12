cmake_minimum_required (VERSION 3.7.0 FATAL_ERROR)
set (PROJECT_NAME "Panoptes")
project (${PROJECT_NAME})

message (STATUS "Running CMake version ${CMAKE_VERSION}")
message (STATUS "Compiler version is: ${CMAKE_CXX_COMPILER_VERSION}")

#
# Add some cxx flags
#

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#
# Define some options
#

option (BUILD_TESTS "Build the ${PROJECT_NAME} test binaries" OFF)

#
# Add a definition for each OS.
#

if (WIN32)
    add_definitions ("-D__WINDOWS__")
endif (WIN32)
if (UNIX)
    if (APPLE)
        add_definitions ("-D__APPLE__")
    else (APPLE)
        add_definitions ("-D__LINUX__")
    endif(APPLE)
endif (UNIX)

#
# Configure Sources.
#

set (PANOPTES_INCLUDE_DIR       "${CMAKE_SOURCE_DIR}/include")
set (PANOPTES_SRC               "${CMAKE_SOURCE_DIR}/src")
set (PANOPTES_CONSOLE           "${CMAKE_SOURCE_DIR}/console")
set (PANOPTES_TESTING           "${CMAKE_SOURCE_DIR}/test")
set (PANOPTES_LIBRARY_NAME      "PanoptesFW")
set (CMAKE_MODULE_PATH          "${CMAKE_SOURCE_DIR}/cmake/modules")

if (APPLE)
    find_library(CORE_SERVICES CoreServices)
    if (NOT CORE_SERVICES)
        message(FATAL ERROR "Did not find required macOS frameworks.")
    endif (NOT CORE_SERVICES)
endif (APPLE)

add_subdirectory (${PANOPTES_SRC})
add_subdirectory (${PANOPTES_CONSOLE})
if (BUILD_TESTS)
    enable_testing()
    add_subdirectory (${PANOPTES_TESTING})
endif (BUILD_TESTS)
