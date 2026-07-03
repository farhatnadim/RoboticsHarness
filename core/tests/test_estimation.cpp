#include "harness/estimation/kalman.hpp"

#include <Eigen/Eigenvalues>

#include <gtest/gtest.h>

namespace {

using KF = harness::estimation::KalmanFilter<2, 1>;

KF make_cv_filter(double dt) {
    Eigen::Matrix2d A;
    A << 1.0, dt, 0.0, 1.0;
    Eigen::Matrix<double, 1, 2> H;
    H << 1.0, 0.0;
    const Eigen::Matrix2d Q = 0.01 * Eigen::Matrix2d::Identity();
    const Eigen::Matrix<double, 1, 1> R = Eigen::Matrix<double, 1, 1>::Constant(0.1);
    return KF{A, H, Q, R, Eigen::Vector2d::Zero(), Eigen::Matrix2d::Identity()};
}

} // namespace

TEST(Kalman, UpdateReducesCovariance) {
    KF kf = make_cv_filter(0.1);
    kf.predict();
    const double trace_before = kf.covariance().trace();
    kf.update(Eigen::Matrix<double, 1, 1>::Constant(0.5));
    EXPECT_LT(kf.covariance().trace(), trace_before);
}

TEST(Kalman, ConvergesToConstantVelocityTrack) {
    const double dt = 0.1;
    const double true_velocity = 3.0;
    KF kf = make_cv_filter(dt);
    for (int k = 1; k <= 200; ++k) {
        kf.predict();
        const double true_pos = true_velocity * static_cast<double>(k) * dt;
        kf.update(Eigen::Matrix<double, 1, 1>::Constant(true_pos));
    }
    EXPECT_NEAR(kf.state()(1), true_velocity, 0.05);
    EXPECT_NEAR(kf.state()(0), true_velocity * 200.0 * 0.1, 0.05);
}

TEST(Kalman, CovarianceStaysSymmetricPsd) {
    KF kf = make_cv_filter(0.1);
    for (int k = 0; k < 100; ++k) {
        kf.predict();
        kf.update(Eigen::Matrix<double, 1, 1>::Constant(1.0));
        const Eigen::Matrix2d P = kf.covariance();
        EXPECT_LT((P - P.transpose()).norm(), 1e-12);
        const Eigen::SelfAdjointEigenSolver<Eigen::Matrix2d> solver(P);
        EXPECT_GE(solver.eigenvalues().minCoeff(), -1e-12);
    }
}
