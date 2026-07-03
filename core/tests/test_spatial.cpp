#include "harness/spatial/se3.hpp"
#include "harness/spatial/so3.hpp"

#include <Eigen/Geometry>

#include <gtest/gtest.h>

#include <numbers>

namespace hs = harness::spatial;

TEST(So3, HatVeeRoundTrip) {
    const Eigen::Vector3d w(0.3, -0.7, 1.2);
    EXPECT_TRUE(hs::vee(hs::hat(w)).isApprox(w));
    const Eigen::Matrix3d W = hs::hat(w);
    EXPECT_TRUE((W + W.transpose()).isZero());
}

TEST(So3, HatMatchesCrossProduct) {
    const Eigen::Vector3d w(0.5, 1.0, -2.0);
    const Eigen::Vector3d v(-1.0, 0.3, 0.7);
    EXPECT_TRUE((hs::hat(w) * v).isApprox(w.cross(v)));
}

TEST(So3, ExpMatchesAngleAxis) {
    const Eigen::Vector3d axis = Eigen::Vector3d(1.0, 2.0, -0.5).normalized();
    const double angle = 0.9;
    const Eigen::Matrix3d expected = Eigen::AngleAxisd(angle, axis).toRotationMatrix();
    EXPECT_TRUE(hs::exp_so3(angle * axis).isApprox(expected, 1e-12));
}

TEST(So3, LogExpRoundTrip) {
    const Eigen::Vector3d w(0.3, -0.2, 0.5);
    EXPECT_TRUE(hs::log_so3(hs::exp_so3(w)).isApprox(w, 1e-9));
}

TEST(So3, SmallAngleRoundTrip) {
    const Eigen::Vector3d w(1e-10, -2e-10, 1e-10);
    const Eigen::Matrix3d R = hs::exp_so3(w);
    EXPECT_TRUE(R.isApprox(Eigen::Matrix3d::Identity(), 1e-9));
    EXPECT_LT((hs::log_so3(R) - w).norm(), 1e-15);
}

TEST(So3, NearPiRoundTrip) {
    const Eigen::Vector3d axis = Eigen::Vector3d(0.2, -1.0, 0.4).normalized();
    const double angle = std::numbers::pi - 1e-7;
    const Eigen::Matrix3d R = hs::exp_so3(angle * axis);
    const Eigen::Vector3d w = hs::log_so3(R);
    EXPECT_NEAR(w.norm(), angle, 1e-5);
    EXPECT_TRUE(hs::exp_so3(w).isApprox(R, 1e-6));
}

TEST(So3, ExactlyPiRoundTrip) {
    const Eigen::Vector3d axis = Eigen::Vector3d(0.2, -1.0, 0.4).normalized();
    const Eigen::Matrix3d R = hs::exp_so3(std::numbers::pi * axis);
    const Eigen::Vector3d w = hs::log_so3(R);
    EXPECT_NEAR(w.norm(), std::numbers::pi, 1e-9);
    // exp(pi*a) == exp(-pi*a), so either sign of the recovered axis is a
    // valid log; round-tripping through exp is the sign-agnostic check.
    EXPECT_TRUE(hs::exp_so3(w).isApprox(R, 1e-9));
}

TEST(So3, LeftJacobianInverseIsInverse) {
    const Eigen::Vector3d phi(0.4, -0.3, 0.8);
    const Eigen::Matrix3d JJinv = hs::left_jacobian_so3(phi) * hs::left_jacobian_inv_so3(phi);
    EXPECT_TRUE(JJinv.isApprox(Eigen::Matrix3d::Identity(), 1e-10));
}

TEST(Se3, ComposeInverseIsIdentity) {
    const hs::SE3 T{hs::exp_so3(Eigen::Vector3d(0.2, 0.1, -0.3)), Eigen::Vector3d(1.0, -2.0, 0.5)};
    const hs::SE3 I = T * T.inverse();
    EXPECT_TRUE(I.rotation().isApprox(Eigen::Matrix3d::Identity(), 1e-12));
    EXPECT_LT(I.translation().norm(), 1e-12);
}

TEST(Se3, ExpLogRoundTrip) {
    hs::Vector6d xi;
    xi << 0.1, 0.2, 0.3, 0.2, -0.1, 0.3;
    EXPECT_TRUE(hs::exp_se3(xi).log().isApprox(xi, 1e-9));
}

TEST(Se3, ActMatchesManualTransform) {
    const Eigen::Matrix3d R = hs::exp_so3(Eigen::Vector3d(0.0, 0.0, std::numbers::pi / 2.0));
    const hs::SE3 T{R, Eigen::Vector3d(1.0, 0.0, 0.0)};
    EXPECT_TRUE(
        T.act(Eigen::Vector3d(1.0, 0.0, 0.0)).isApprox(Eigen::Vector3d(1.0, 1.0, 0.0), 1e-12));
}

// Compiled with EIGEN_RUNTIME_NO_MALLOC on the test target: proves the hot
// path of the spatial module performs no Eigen heap allocation.
TEST(RealtimeGuard, SpatialOpsDoNotAllocate) {
    Eigen::internal::set_is_malloc_allowed(false);
    const Eigen::Matrix3d R = hs::exp_so3(Eigen::Vector3d(0.1, -0.2, 0.3));
    const Eigen::Vector3d w = hs::log_so3(R);
    const hs::SE3 T{R, Eigen::Vector3d(0.5, 0.5, 0.5)};
    const hs::Vector6d xi = T.log();
    Eigen::internal::set_is_malloc_allowed(true);
    EXPECT_LT((hs::exp_so3(w) - R).norm(), 1e-12);
    EXPECT_TRUE(hs::exp_se3(xi).rotation().isApprox(R, 1e-9));
}
