#pragma once

#include <Eigen/Core>

namespace cart_pole {

constexpr double kCartMass = 1.0;   // kg
constexpr double kPoleMass = 0.1;   // kg
constexpr double kPoleLength = 0.5; // m, pivot to point mass
constexpr double kGravity = 9.81;   // m/s^2
constexpr double kDt = 0.02;        // s, 50 Hz control loop

// Linearized cart-pole dynamics about the upright equilibrium, discretized
// via forward Euler (A_d = I + dt*A_c, B_d = dt*B_c). State
// x = [cart position, cart velocity, pole angle from vertical, pole angular
// velocity]; pole modeled as a point mass at distance kPoleLength from the
// pivot. Derived via Lagrangian mechanics then linearized at theta = 0.
[[nodiscard]] inline Eigen::Matrix4d discrete_a() {
    Eigen::Matrix4d a_c = Eigen::Matrix4d::Zero();
    a_c(0, 1) = 1.0;
    a_c(1, 2) = -kPoleMass * kGravity / kCartMass;
    a_c(2, 3) = 1.0;
    a_c(3, 2) = (kCartMass + kPoleMass) * kGravity / (kCartMass * kPoleLength);
    return Eigen::Matrix4d::Identity() + kDt * a_c;
}

[[nodiscard]] inline Eigen::Vector4d discrete_b() {
    Eigen::Vector4d b_c;
    b_c << 0.0, 1.0 / kCartMass, 0.0, -1.0 / (kCartMass * kPoleLength);
    return kDt * b_c;
}

// LQR weights: penalize angle and angular velocity more than cart position
// and velocity, since keeping the pole upright is the actual goal.
[[nodiscard]] inline Eigen::Matrix4d state_cost() {
    Eigen::Matrix4d q = Eigen::Matrix4d::Identity();
    q(2, 2) = 10.0;
    q(3, 3) = 10.0;
    return q;
}

[[nodiscard]] inline Eigen::Matrix<double, 1, 1> input_cost() {
    return Eigen::Matrix<double, 1, 1>::Constant(0.1);
}

} // namespace cart_pole
