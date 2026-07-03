# Opt a target into the realtime tier. /rt-check discovers realtime targets by
# grepping the tree for calls to this function, so always call it literally:
#   harness_enable_realtime(my_target)
function(harness_enable_realtime target)
    target_link_libraries(${target} PRIVATE harness::realtime_flags)
    set_target_properties(${target} PROPERTIES HARNESS_TIER realtime)
    set_property(GLOBAL APPEND PROPERTY HARNESS_REALTIME_TARGETS ${target})
endfunction()
