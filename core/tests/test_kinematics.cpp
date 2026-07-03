#include "harness/kinematics/planar_chain.hpp"

#include <gtest/gtest.h>

#include <numbers>

using harness::kinematics::PlanarChain;

TEST(PlanarChain, TwoLinkForwardKinematics) {
    const PlanarChain<2> chain({1.0, 1.0});
    // First link straight up, second link bends back to horizontal.
    const Eigen::Vector2d q(std::numbers::pi / 2.0, -std::numbers::pi / 2.0);
    const Eigen::Vector3d pose = chain.fk(q);
    EXPECT_NEAR(pose(0), 1.0, 1e-12);
    EXPECT_NEAR(pose(1), 1.0, 1e-12);
    EXPECT_NEAR(pose(2), 0.0, 1e-12);
}

TEST(PlanarChain, StretchedChainReach) {
    const PlanarChain<3> chain({1.0, 0.8, 0.5});
    const Eigen::Vector3d pose = chain.fk(Eigen::Vector3d::Zero());
    EXPECT_NEAR(pose(0), 2.3, 1e-12);
    EXPECT_NEAR(pose(1), 0.0, 1e-12);
}

TEST(PlanarChain, JacobianMatchesFiniteDifferences) {
    const PlanarChain<3> chain({1.0, 0.8, 0.5});
    const Eigen::Vector3d q(0.3, -0.4, 0.9);
    const Eigen::Matrix3d J = chain.jacobian(q);
    const double h = 1e-7;
    for (int j = 0; j < 3; ++j) {
        Eigen::Vector3d q_plus = q;
        Eigen::Vector3d q_minus = q;
        q_plus(j) += h;
        q_minus(j) -= h;
        const Eigen::Vector3d fd = (chain.fk(q_plus) - chain.fk(q_minus)) / (2.0 * h);
        EXPECT_LT((J.col(j) - fd).norm(), 1e-6);
    }
}

TEST(PlanarChain, IkConvergesToReachableTarget) {
    const PlanarChain<3> chain({1.0, 0.8, 0.5});
    const Eigen::Vector2d target(1.2, 0.9);
    const auto q = chain.solve_ik(Eigen::Vector3d(0.2, 0.2, 0.2), target);
    ASSERT_TRUE(q.has_value());
    EXPECT_LT((chain.fk(*q).head<2>() - target).norm(), 1e-5);
}

TEST(PlanarChain, IkReturnsNulloptForUnreachableTarget) {
    const PlanarChain<2> chain({1.0, 1.0});
    EXPECT_FALSE(chain.solve_ik(Eigen::Vector2d(0.1, 0.1), Eigen::Vector2d(5.0, 0.0)).has_value());
}
