# Goals — RoboticsHarness

## Goals

- [x] Core library v0 builds and passes all tests under ASan/UBSan (spatial, control, estimation, trajectory, kinematics)
- [x] Realtime tier verification in place (compile check + symbol scan + /rt-check skill)
- [x] Problem scaffolding works end-to-end (/new-problem creates a building, tested project)
- [x] Self-evolution loop live (hooks journal failures; /evolve produces reviewed harness changes)
- [x] First real robotics problem attacked through the harness (`problems/cart-pole`:
      LQR-balanced inverted pendulum)

## Backlog

- [x] Install clang toolchain and switch `CMakePresets.json` to `clang++` (done: clang
      18.1.3 installed, all presets rebuilt and verified green)
- [x] Make `capture_failure.py` stop over-capturing on chained commands / quoted text
      (done in the 2026-07-03 council review: build verbs anchored to command position,
      journal-touching commands skipped, decoded stdout/stderr classified, excerpt
      windowed around the first marker; exit codes remain unavailable in the hook's
      tool_response schema, so classification stays marker-based by design)
- [ ] Optional-returning factories (or equivalent guarded construction) for
      `QuinticPolynomial` (`duration == 0`) and `TrapezoidalProfile`
      (`v_max/a_max <= 0`) — constructors currently propagate silent NaN on
      precondition violation, against the errors-via-std::optional realtime rule
- [ ] Kalman test with NZ > 1 (gain/transpose logic only validated for scalar
      measurements so far); SE3 log round-trip test for a near-pi rotation
- [ ] Extended Kalman filter (EKF) with numeric-Jacobian fallback
- [ ] 3D serial-chain kinematics (product of exponentials)
- [ ] Continuous-time LQR via CARE solver
- [ ] Cubic splines / minimum-jerk multi-segment trajectories
- [ ] IMU orientation filters (complementary, Madgwick)
- [ ] google-benchmark wiring + /bench skill
- [ ] ROS 2 adapter layer (thin wrappers over core types)
- [ ] SE3 adjoint and Jacobians of exp/log
- [ ] CI workflow (build + test + rt-check on push)
