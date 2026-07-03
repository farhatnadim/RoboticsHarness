# cart-pole

LQR balancing of an inverted pendulum on a cart.

Linearized cart-pole dynamics (pole modeled as a point mass at distance
`kPoleLength` from the pivot, state `[cart position, cart velocity, pole angle
from vertical, pole angular velocity]`), discretized via forward Euler at 50 Hz
and stabilized with `harness::control::dlqr`. Model, discretization, and LQR
weights live in `include/cart_pole_model.hpp`, shared by both `src/main.cpp`
(prints a settling trajectory from a 0.1 rad tilt) and `tests/test_smoke.cpp`
(asserts closed-loop stability and convergence).

Part of RoboticsHarness — links `harness::core`. Build and test from the repo root:

```sh
cmake --build --preset debug --target cart-pole_app cart-pole_tests
ctest --preset debug -R cart-pole
```
