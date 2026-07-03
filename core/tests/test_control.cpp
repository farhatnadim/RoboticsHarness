#include "harness/control/lqr.hpp"
#include "harness/control/pid.hpp"

#include <Eigen/Eigenvalues>

#include <gtest/gtest.h>

#include <complex>

using harness::control::Pid;

TEST(Pid, ProportionalOnly) {
    Pid pid({.kp = 2.0});
    EXPECT_DOUBLE_EQ(pid.update(1.5, 0.1), 3.0);
    EXPECT_DOUBLE_EQ(pid.update(-0.5, 0.1), -1.0);
}

TEST(Pid, IntegralWindupClamp) {
    Pid pid({.ki = 1.0}, {.i_min = -0.5, .i_max = 0.5});
    double out = 0.0;
    for (int i = 0; i < 100; ++i) {
        out = pid.update(10.0, 0.1);
    }
    EXPECT_DOUBLE_EQ(pid.integral(), 0.5);
    EXPECT_DOUBLE_EQ(out, 0.5);
}

TEST(Pid, OutputSaturation) {
    Pid pid({.kp = 100.0}, {.out_min = -1.0, .out_max = 1.0});
    EXPECT_DOUBLE_EQ(pid.update(5.0, 0.1), 1.0);
    EXPECT_DOUBLE_EQ(pid.update(-5.0, 0.1), -1.0);
}

TEST(Pid, DerivativeOnError) {
    Pid pid({.kd = 2.0});
    EXPECT_DOUBLE_EQ(pid.update(1.0, 0.1), 0.0); // first call: no previous error
    EXPECT_DOUBLE_EQ(pid.update(2.0, 0.1), 2.0 * (2.0 - 1.0) / 0.1);
}

TEST(Pid, ResetClearsState) {
    Pid pid({.ki = 1.0, .kd = 1.0});
    const double first = pid.update(1.0, 0.1);
    (void)pid.update(3.0, 0.1);
    pid.reset();
    EXPECT_DOUBLE_EQ(pid.update(1.0, 0.1), first);
}

TEST(Pid, NonPositiveDtDegradesToClampedP) {
    Pid pid({.kp = 2.0, .ki = 1.0, .kd = 1.0}, {.out_min = -3.0, .out_max = 3.0});
    EXPECT_DOUBLE_EQ(pid.update(1.0, 0.0), 2.0);  // pure P response
    EXPECT_DOUBLE_EQ(pid.update(5.0, -0.1), 3.0); // clamped, negative dt
    EXPECT_DOUBLE_EQ(pid.integral(), 0.0);        // state untouched by either call
    // First valid update still sees no previous error (zero derivative).
    EXPECT_DOUBLE_EQ(pid.update(1.0, 0.1), 2.0 + 1.0 * 0.1);
}

TEST(Lqr, DoubleIntegratorStabilizes) {
    const double dt = 0.1;
    Eigen::Matrix2d A;
    A << 1.0, dt, 0.0, 1.0;
    Eigen::Matrix<double, 2, 1> B;
    B << 0.5 * dt * dt, dt;
    const Eigen::Matrix2d Q = Eigen::Matrix2d::Identity();
    const Eigen::Matrix<double, 1, 1> R = Eigen::Matrix<double, 1, 1>::Identity();
    const auto K = harness::control::dlqr<2, 1>(A, B, Q, R);
    ASSERT_TRUE(K.has_value());

    const Eigen::Matrix2d Acl = A - B * (*K);
    const Eigen::Vector2cd eigs = Acl.eigenvalues();
    EXPECT_LT(std::abs(eigs(0)), 1.0);
    EXPECT_LT(std::abs(eigs(1)), 1.0);

    Eigen::Vector2d x(1.0, 0.0);
    for (int i = 0; i < 300; ++i) {
        x = Acl * x;
    }
    EXPECT_LT(x.norm(), 1e-3);
}

TEST(Lqr, UnstabilizableReturnsNullopt) {
    const Eigen::Matrix2d A = 2.0 * Eigen::Matrix2d::Identity();
    const Eigen::Matrix<double, 2, 1> B = Eigen::Matrix<double, 2, 1>::Zero();
    const Eigen::Matrix2d Q = Eigen::Matrix2d::Identity();
    const Eigen::Matrix<double, 1, 1> R = Eigen::Matrix<double, 1, 1>::Identity();
    EXPECT_FALSE((harness::control::dlqr<2, 1>(A, B, Q, R, 50).has_value()));
}

// dlqr's .ldlt().solve() routes through Eigen's generic blocked-GEMM internals,
// which statically reference operator new/malloc even for fixed 2x2 matrices
// (confirmed via objdump: those symbols exist in the object but are never
// called for our sizes). This test proves it at runtime instead of relying on
// static symbol presence, which is unreliable for Eigen decomposition code.
TEST(RealtimeGuard, LqrDoesNotAllocate) {
    Eigen::Matrix2d A;
    A << 1.0, 0.1, 0.0, 1.0;
    Eigen::Matrix<double, 2, 1> B;
    B << 0.005, 0.1;
    const Eigen::Matrix2d Q = Eigen::Matrix2d::Identity();
    const Eigen::Matrix<double, 1, 1> R = Eigen::Matrix<double, 1, 1>::Identity();

    Eigen::internal::set_is_malloc_allowed(false);
    const auto K = harness::control::dlqr<2, 1>(A, B, Q, R);
    Eigen::internal::set_is_malloc_allowed(true);
    ASSERT_TRUE(K.has_value());
}
