// Verifies the LQR-stabilized cart-pole model actually converges to upright
// from a small initial tilt, not just that it links against harness::core.

#include "harness/control/lqr.hpp"

#include <Eigen/Eigenvalues>

#include <gtest/gtest.h>

#include <cmath>

#include "cart_pole_model.hpp"

// Guards against sign errors in the linearization: an inverted pendulum
// diverges monotonically from a small tilt under zero input (cosh growth),
// while a wrongly-signed (hanging) model oscillates back through zero. A
// spectral-radius check alone can't tell them apart — forward Euler makes
// the hanging model's poles slightly unstable too.
TEST(CartPole, OpenLoopUprightIsUnstable) {
    using namespace cart_pole;
    const Eigen::Matrix4d a = discrete_a();
    Eigen::Vector4d x;
    x << 0.0, 0.0, 0.01, 0.0;
    double prev_angle = x(2);
    for (int i = 0; i < 50; ++i) { // 1 s at 50 Hz
        x = a * x;
        EXPECT_GE(x(2), prev_angle); // never swings back toward upright
        prev_angle = x(2);
    }
    EXPECT_GT(x(2), 0.05); // cosh(omega*t) growth: >5x initial tilt in 1 s
}

TEST(CartPole, LqrStabilizesFromSmallTilt) {
    using namespace cart_pole;
    const Eigen::Matrix4d a = discrete_a();
    const Eigen::Vector4d b = discrete_b();

    const auto k = harness::control::dlqr<4, 1>(a, b, state_cost(), input_cost());
    ASSERT_TRUE(k.has_value());

    const Eigen::Matrix4d a_cl = a - b * (*k);
    const Eigen::Vector4cd eigs = a_cl.eigenvalues();
    for (int i = 0; i < 4; ++i) {
        EXPECT_LT(std::abs(eigs(i)), 1.0);
    }

    Eigen::Vector4d x;
    x << 0.0, 0.0, 0.1, 0.0;
    // Cart position/velocity are lightly weighted in Q, so they settle more
    // slowly than the angle; 500 steps (10s) is needed to clear 1e-3 (5s only
    // reaches ~4e-3, still decaying).
    for (int i = 0; i < 500; ++i) {
        x = a_cl * x;
    }
    EXPECT_LT(x.norm(), 1e-3);
}
