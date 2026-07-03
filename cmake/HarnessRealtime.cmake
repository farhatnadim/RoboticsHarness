# Opt a target into the realtime tier. Every opted-in target is recorded in
# the HARNESS_REALTIME_TARGETS global property, which the top-level
# CMakeLists.txt dumps to <build-dir>/realtime_targets.txt at configure time —
# that file is /rt-check's authoritative target list (a grep of the source
# tree would also match commented-out calls and prose).
function(harness_enable_realtime target)
    target_link_libraries(${target} PRIVATE harness::realtime_flags)
    set_target_properties(${target} PROPERTIES HARNESS_TIER realtime)
    set_property(GLOBAL APPEND PROPERTY HARNESS_REALTIME_TARGETS ${target})
endfunction()
