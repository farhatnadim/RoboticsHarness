include(FetchContent)

# Eigen: prefer the system package, fetch pinned 3.4.0 otherwise.
set(EIGEN_BUILD_DOC OFF CACHE BOOL "" FORCE)
set(EIGEN_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(EIGEN_BUILD_PKGCONFIG OFF CACHE BOOL "" FORCE)
FetchContent_Declare(Eigen3
    GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
    GIT_TAG 3.4.0
    GIT_SHALLOW TRUE
    FIND_PACKAGE_ARGS 3.4 CONFIG
)

# GoogleTest: always fetched and pinned, never the system copy.
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(Eigen3 googletest)

# When Eigen is built from source (no system package found), mark its headers
# as SYSTEM so Eigen-internal warnings don't trip our -Werror.
if(TARGET eigen)
    get_target_property(_eigen_dirs eigen INTERFACE_INCLUDE_DIRECTORIES)
    if(_eigen_dirs)
        set_target_properties(eigen PROPERTIES
            INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${_eigen_dirs}")
    endif()
endif()

option(HARNESS_BUILD_BENCHMARKS "Build google-benchmark microbenchmarks" OFF)
# Benchmark wiring is backlog (GOALS.md); the option exists so targets can gate on it.
