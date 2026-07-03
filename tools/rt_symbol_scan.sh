#!/usr/bin/env bash
# Scan a target's object files for symbols forbidden in the realtime tier:
# allocating operator new/new[] (NOT placement new, which never allocates and
# is used harmlessly by std::optional/Eigen internals), exception machinery
# (__cxa_throw, __cxa_allocate_exception), and RTTI typeinfo.
#
# Uses demangled names (nm -C) with anchored patterns so allocating
# "operator new(unsigned long)" is distinguished from placement
# "operator new(unsigned long, void*)" — a plain substring match on the
# mangled _Znwm would incorrectly flag the latter too.
#
# Usage: tools/rt_symbol_scan.sh <build-dir> <target-name>
# Exit: 0 clean, 1 violations found, 2 no object files for target.
set -u

build_dir="${1:?usage: rt_symbol_scan.sh <build-dir> <target>}"
target="${2:?usage: rt_symbol_scan.sh <build-dir> <target>}"

# Anchored at end-of-line so "operator new(unsigned long)" doesn't match the
# placement form "operator new(unsigned long, void*)".
PATTERN='operator new(\[\])?\(unsigned long\)$|__cxa_throw|__cxa_allocate_exception|typeinfo for '

status=0
found=0
while IFS= read -r obj; do
    found=1
    hits=$(nm -C "$obj" 2>/dev/null | grep -E "$PATTERN" || true)
    if [ -n "$hits" ]; then
        echo "RT VIOLATION in ${obj}:"
        echo "$hits"
        status=1
    fi
done < <(find "$build_dir" -path "*CMakeFiles/${target}.dir/*" -name '*.o')

if [ "$found" -eq 0 ]; then
    echo "error: no object files found for target '${target}' under ${build_dir}" >&2
    echo "hint: build the target first (cmake --build --preset debug --target ${target})" >&2
    exit 2
fi

if [ "$status" -eq 0 ]; then
    echo "OK: no exception/RTTI/heap-allocation symbols in target '${target}'"
fi
exit "$status"
