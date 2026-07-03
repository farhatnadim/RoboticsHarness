# 3.25 floor: CMakePresets.json uses schema version 6, which older CMake rejects.
cmake_minimum_required(VERSION 3.25)

project(RoboticsHarness VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

include(cmake/HarnessFlags.cmake)
include(cmake/HarnessRealtime.cmake)
include(cmake/Dependencies.cmake)

add_subdirectory(core)

# Each problems/<name>/ with a CMakeLists.txt is an independent sub-project.
# _template holds unrendered *.in files and is never built.
file(GLOB problem_dirs LIST_DIRECTORIES true CONFIGURE_DEPENDS
     ${CMAKE_CURRENT_SOURCE_DIR}/problems/*)
foreach(problem_dir IN LISTS problem_dirs)
    get_filename_component(problem_name ${problem_dir} NAME)
    if(NOT problem_name STREQUAL "_template" AND EXISTS ${problem_dir}/CMakeLists.txt)
        add_subdirectory(${problem_dir})
    endif()
endforeach()

# Authoritative realtime-target list for /rt-check (see HarnessRealtime.cmake).
get_property(harness_realtime_targets GLOBAL PROPERTY HARNESS_REALTIME_TARGETS)
list(JOIN harness_realtime_targets "\n" harness_realtime_targets_lines)
file(WRITE ${CMAKE_BINARY_DIR}/realtime_targets.txt "${harness_realtime_targets_lines}\n")
