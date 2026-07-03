#include "harness/trajectory/quintic.hpp"
#include "harness/trajectory/trapezoidal.hpp"

#include <gtest/gtest.h>

using harness::trajectory::QuinticPolynomial;
using harness::trajectory::TrapezoidalProfile;

TEST(Quintic, BoundaryConditionsExact) {
    const QuinticPolynomial q(0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 2.0);
    EXPECT_NEAR(q.position(0.0), 0.0, 1e-12);
    EXPECT_NEAR(q.velocity(0.0), 0.0, 1e-12);
    EXPECT_NEAR(q.acceleration(0.0), 0.0, 1e-12);
    EXPECT_NEAR(q.position(2.0), 1.0, 1e-9);
    EXPECT_NEAR(q.velocity(2.0), 0.0, 1e-9);
    EXPECT_NEAR(q.acceleration(2.0), 0.0, 1e-9);
}

TEST(Quintic, NonRestBoundaryConditions) {
    const QuinticPolynomial q(1.0, -0.5, 0.2, -2.0, 1.5, -0.3, 3.0);
    EXPECT_NEAR(q.position(0.0), 1.0, 1e-12);
    EXPECT_NEAR(q.velocity(0.0), -0.5, 1e-12);
    EXPECT_NEAR(q.acceleration(0.0), 0.2, 1e-12);
    EXPECT_NEAR(q.position(3.0), -2.0, 1e-9);
    EXPECT_NEAR(q.velocity(3.0), 1.5, 1e-9);
    EXPECT_NEAR(q.acceleration(3.0), -0.3, 1e-9);
}

TEST(Quintic, VelocityIsDerivativeOfPosition) {
    const QuinticPolynomial q(0.0, 0.2, 0.0, 1.0, -0.1, 0.0, 2.0);
    const double h = 1e-6;
    for (const double t : {0.3, 0.7, 1.1, 1.9}) {
        const double fd = (q.position(t + h) - q.position(t - h)) / (2.0 * h);
        EXPECT_NEAR(q.velocity(t), fd, 1e-5);
    }
}

TEST(Trapezoidal, FullProfileTimingAndDistance) {
    // d=10, v_max=2, a_max=1: t_acc=2, cruise covers 6 at v=2 -> t_c=3, total 7.
    const TrapezoidalProfile p(10.0, 2.0, 1.0);
    EXPECT_NEAR(p.duration(), 7.0, 1e-12);
    EXPECT_NEAR(p.position(p.duration()), 10.0, 1e-9);
    EXPECT_NEAR(p.position(0.0), 0.0, 1e-12);
    EXPECT_NEAR(p.velocity(3.5), 2.0, 1e-12);
}

TEST(Trapezoidal, VelocityNeverExceedsLimit) {
    const TrapezoidalProfile p(10.0, 2.0, 1.0);
    for (int i = 0; i <= 100; ++i) {
        const double t = p.duration() * static_cast<double>(i) / 100.0;
        EXPECT_LE(p.velocity(t), 2.0 + 1e-12);
        EXPECT_GE(p.velocity(t), -1e-12);
    }
}

TEST(Trapezoidal, TriangularDegenerateCase) {
    // d=1, v_max=10, a_max=1: never reaches v_max; t_acc=1, peak v=1, total 2.
    const TrapezoidalProfile p(1.0, 10.0, 1.0);
    EXPECT_NEAR(p.duration(), 2.0, 1e-12);
    EXPECT_NEAR(p.position(p.duration()), 1.0, 1e-9);
    EXPECT_NEAR(p.velocity(1.0), 1.0, 1e-9);
}

TEST(Trapezoidal, NegativeDistanceMirrors) {
    const TrapezoidalProfile p(-10.0, 2.0, 1.0);
    EXPECT_NEAR(p.position(p.duration()), -10.0, 1e-9);
    EXPECT_LE(p.velocity(3.5), -2.0 + 1e-12);
}

TEST(Trapezoidal, QueriesOutsideDomainClamp) {
    const TrapezoidalProfile p(10.0, 2.0, 1.0);
    EXPECT_NEAR(p.position(-1.0), 0.0, 1e-12);
    EXPECT_NEAR(p.position(100.0), 10.0, 1e-9);
    EXPECT_NEAR(p.velocity(100.0), 0.0, 1e-12);
}
