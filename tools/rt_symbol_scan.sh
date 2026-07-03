#!/usr/bin/env bash
# Scan a target's object files for symbols forbidden in the realtime tier.
#
# HARD violations (exit 1): exception machinery (__cxa_throw,
# __cxa_allocate_exception) and RTTI typeinfo. These are reliable signals of
# actual exception/RTTI codegen.
#
# SOFT advisory (printed, does not fail): allocating operator new/new[].
# Eigen's generic blocked-GEMM internals (used by .ldlt(), .inverse(), and
# similar decompositions) statically reference operator new/malloc even for
# small fixed-size matrices that never actually take that code path at
# runtime — confirmed by objdump: the relocations exist in
# triangular_solve_matrix/gemm_pack_rhs template instantiations regardless of
# matrix size. A static symbol scan cannot distinguish "referenced by a
# compiled-but-never-taken branch" from "actually called", so flagging any
# operator-new reference as a hard failure would reject legitimate,
# non-allocating use of Eigen decompositions. The reliable way to verify a
# specific piece of code never allocates at runtime is a malloc-guard test:
#   Eigen::internal::set_is_malloc_allowed(false);
#   ... call the code under test ...
#   Eigen::internal::set_is_malloc_allowed(true);
# with EIGEN_RUNTIME_NO_MALLOC defined on the test target (see
# RealtimeGuard.LqrDoesNotAllocate / RealtimeGuard.KalmanDoesNotAllocate in
# core/tests/ for the pattern). Placement new (operator new(unsigned long,
# void*)) is excluded entirely — it never allocates.
#
# Usage: tools/rt_symbol_scan.sh <build-dir> <target-name>
# Exit: 0 no hard violations, 1 hard violations found, 2 no object files for target.
set -u

build_dir="${1:?usage: rt_symbol_scan.sh <build-dir> <target>}"
target="${2:?usage: rt_symbol_scan.sh <build-dir> <target>}"

HARD_PATTERN='__cxa_throw|__cxa_allocate_exception|typeinfo for '
# Anchored at end-of-line so "operator new(unsigned long)" doesn't match the
# placement form "operator new(unsigned long, void*)".
SOFT_PATTERN='operator new(\[\])?\(unsigned long\)$'

status=0
found=0
while IFS= read -r obj; do
    found=1
    demangled=$(nm -C "$obj" 2>/dev/null)
    hard_hits=$(echo "$demangled" | grep -E "$HARD_PATTERN" || true)
    soft_hits=$(echo "$demangled" | grep -E "$SOFT_PATTERN" || true)
    if [ -n "$hard_hits" ]; then
        echo "RT VIOLATION in ${obj}:"
        echo "$hard_hits"
        status=1
    fi
    if [ -n "$soft_hits" ]; then
        echo "RT ADVISORY in ${obj} (allocating operator new referenced; verify with a runtime malloc-guard test, see script header):"
        echo "$soft_hits"
    fi
done < <(find "$build_dir" -path "*CMakeFiles/${target}.dir/*" -name '*.o')

if [ "$found" -eq 0 ]; then
    echo "error: no object files found for target '${target}' under ${build_dir}" >&2
    echo "hint: build the target first (cmake --build --preset debug --target ${target})" >&2
    exit 2
fi

if [ "$status" -eq 0 ]; then
    echo "OK: no exception/RTTI symbols in target '${target}'"
fi
exit "$status"
