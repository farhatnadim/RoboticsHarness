add_library(harness_core INTERFACE)
add_library(harness::core ALIAS harness_core)

target_include_directories(harness_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(harness_core INTERFACE Eigen3::Eigen)

# Proof that every core header stays realtime-clean: this TU is compiled with
# -fno-exceptions -fno-rtti -Werror and EIGEN_NO_MALLOC in every preset.
add_library(core_rt_compile_check OBJECT tests/rt_compile_check.cpp)
target_link_libraries(core_rt_compile_check PRIVATE harness::core)
harness_enable_realtime(core_rt_compile_check)

if(BUILD_TESTING)
    include(GoogleTest)
    add_executable(harness_core_tests
        tests/test_spatial.cpp
        tests/test_control.cpp
        tests/test_estimation.cpp
        tests/test_trajectory.cpp
        tests/test_kinematics.cpp
    )
    target_link_libraries(harness_core_tests PRIVATE
        harness::core
        harness::default_flags
        GTest::gtest_main
    )
    # Enables Eigen::internal::set_is_malloc_allowed for allocation-guard tests.
    target_compile_definitions(harness_core_tests PRIVATE EIGEN_RUNTIME_NO_MALLOC)
    gtest_discover_tests(harness_core_tests)
endif()
