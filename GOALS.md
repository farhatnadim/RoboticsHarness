# Goals — RoboticsHarness

## Goals

- [x] Core library v0 builds and passes all tests under ASan/UBSan (spatial, control, estimation, trajectory, kinematics)
- [x] Realtime tier verification in place (compile check + symbol scan + /rt-check skill)
- [x] Problem scaffolding works end-to-end (/new-problem creates a building, tested project)
- [x] Self-evolution loop live (hooks journal failures; /evolve produces reviewed harness changes)
- [ ] First real robotics problem attacked through the harness

## Backlog

- [x] Install clang toolchain and switch `CMakePresets.json` to `clang++` (done: clang
      18.1.3 installed, all presets rebuilt and verified green)
- [ ] Make `capture_failure.py` classify by actual command exit code instead of regex over
      raw text output (currently over-captures on chained `&&` commands / historical text
      embedded in later output — never blocks work, just adds journal noise)
- [ ] Extended Kalman filter (EKF) with numeric-Jacobian fallback
- [ ] 3D serial-chain kinematics (product of exponentials)
- [ ] Continuous-time LQR via CARE solver
- [ ] Cubic splines / minimum-jerk multi-segment trajectories
- [ ] IMU orientation filters (complementary, Madgwick)
- [ ] google-benchmark wiring + /bench skill
- [ ] ROS 2 adapter layer (thin wrappers over core types)
- [ ] SE3 adjoint and Jacobians of exp/log
- [ ] CI workflow (build + test + rt-check on push)
