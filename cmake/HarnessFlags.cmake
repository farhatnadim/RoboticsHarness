# Two flag tiers as INTERFACE targets. Every target links exactly one of:
#   harness::default_flags  — full warning set as errors, sanitizers in Debug
#   harness::realtime_flags — default tier + no exceptions/RTTI + EIGEN_NO_MALLOC

add_library(harness_default_flags INTERFACE)
add_library(harness::default_flags ALIAS harness_default_flags)

target_compile_features(harness_default_flags INTERFACE cxx_std_20)

target_compile_options(harness_default_flags INTERFACE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
    -Wconversion
    -Wsign-conversion
    -Wshadow
    -Wold-style-cast
    -Wcast-align
    -Wdouble-promotion
    -Wnull-dereference
    -Wnon-virtual-dtor
    -Woverloaded-virtual
    -Wimplicit-fallthrough
    -Wformat=2
    -fstack-protector-strong
    $<$<CONFIG:Debug>:-fsanitize=address,undefined>
    $<$<CONFIG:Debug>:-fno-omit-frame-pointer>
)

target_link_options(harness_default_flags INTERFACE
    $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

target_compile_definitions(harness_default_flags INTERFACE
    $<$<CONFIG:Debug>:_GLIBCXX_ASSERTIONS>
)

add_library(harness_realtime_flags INTERFACE)
add_library(harness::realtime_flags ALIAS harness_realtime_flags)

target_link_libraries(harness_realtime_flags INTERFACE harness_default_flags)

target_compile_options(harness_realtime_flags INTERFACE
    -fno-exceptions
    -fno-rtti
)

target_compile_definitions(harness_realtime_flags INTERFACE
    EIGEN_NO_MALLOC
    HARNESS_REALTIME=1
)
