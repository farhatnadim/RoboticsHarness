# Problem: cart-pole — LQR balancing of an inverted pendulum on a cart

add_executable(cart-pole_app src/main.cpp)
target_link_libraries(cart-pole_app PRIVATE harness::core harness::default_flags)
target_include_directories(cart-pole_app PRIVATE include)

# Realtime tier opt-in (then verify with /rt-check):
# harness_enable_realtime(cart-pole_app)

if(BUILD_TESTING)
    include(GoogleTest)
    add_executable(cart-pole_tests tests/test_smoke.cpp)
    target_link_libraries(cart-pole_tests PRIVATE
        harness::core
        harness::default_flags
        GTest::gtest_main
    )
    target_include_directories(cart-pole_tests PRIVATE include)
    # TEST_PREFIX makes `ctest -R cart-pole` match this problem's tests.
    gtest_discover_tests(cart-pole_tests TEST_PREFIX "cart-pole.")
endif()
