# Goals — RoboticsHarness

## Goals

- [x] Core library v0 builds and passes all tests under ASan/UBSan (spatial, control, estimation, trajectory, kinematics)
- [x] Realtime tier verification in place (compile check + symbol scan + /rt-check skill)
- [x] Problem scaffolding works end-to-end (/new-problem creates a building, tested project)
- [x] Self-evolution loop live (hooks journal failures; /evolve produces reviewed harness changes)
- [ ] First real robotics problem attacked through the harness

## Backlog

- [ ] Install clang toolchain (`sudo apt install clang clang-format clang-tidy`) and switch
      `CMakePresets.json` back to `CMAKE_CXX_COMPILER=g++` → `clang++` (currently on g++
      13.3.0 since clang binaries aren't installed on this machine; see CLAUDE.md Toolchain)
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
