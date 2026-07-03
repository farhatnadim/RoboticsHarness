# Goals — cart-pole

## Goals

- [x] LQR balancing of an inverted pendulum on a cart (linearized point-mass
      model, discrete LQR via `harness::control::dlqr`, converges from a
      0.1 rad tilt within ~8s)
- [ ] Validate the linear controller against the true nonlinear dynamics
      (currently only simulated on the linearized model it was designed on)
- [ ] Larger initial tilt / disturbance-rejection robustness sweep
- [ ] Animate or plot the trajectory instead of printing a state table
