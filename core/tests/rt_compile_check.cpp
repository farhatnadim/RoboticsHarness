// Realtime-tier compile check: this translation unit includes EVERY core
// header and exercises each public entry point. It is built with
// harness::realtime_flags (-fno-exceptions -fno-rtti, EIGEN_NO_MALLOC,
// full warning set as errors) in every preset. If a core header stops being
// realtime-clean, this file fails to build.
//
// When adding a header to core/include/harness/, add it here too (CLAUDE.md
// definition of done).

#include "harness/control/lqr.hpp"
#include "harness/control/pid.hpp"
#include "harness/estimation/kalman.hpp"
#include "harness/kinematics/planar_chain.hpp"
#include "harness/spatial/se3.hpp"
#include "harness/spatial/so3.hpp"
#include "harness/trajectory/quintic.hpp"
#include "harness/trajectory/trapezoidal.hpp"

namespace {

[[nodiscard]] double exercise_all() {
    using namespace harness;

    const Eigen::Vector3d w(0.1, -0.2, 0.3);
    const Eigen::Matrix3d R = spatial::exp_so3(w);
    const spatial::SE3 T{R, Eigen::Vector3d(1.0, 2.0, 3.0)};
    const spatial::Vector6d xi = (T * T.inverse()).log();
    const spatial::SE3 T_exp = spatial::exp_se3(xi);

    control::Pid pid({.kp = 1.0, .ki = 0.1, .kd = 0.01});
    const double u = pid.update(0.5, 0.01);

    Eigen::Matrix2d A;
    A << 1.0, 0.1, 0.0, 1.0;
    Eigen::Matrix<double, 2, 1> B;
    B << 0.005, 0.1;
    const auto K = control::dlqr<2, 1>(A, B, Eigen::Matrix2d::Identity(),
                                       Eigen::Matrix<double, 1, 1>::Identity());

    Eigen::Matrix<double, 1, 2> H;
    H << 1.0, 0.0;
    estimation::KalmanFilter<2, 1> kf(A, H, 0.01 * Eigen::Matrix2d::Identity(),
                                      Eigen::Matrix<double, 1, 1>::Constant(0.1),
                                      Eigen::Vector2d::Zero(), Eigen::Matrix2d::Identity());
    kf.predict();
    kf.update(Eigen::Matrix<double, 1, 1>::Constant(0.5));

    const trajectory::QuinticPolynomial quintic(0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 2.0);
    const trajectory::TrapezoidalProfile trap(10.0, 2.0, 1.0);

    const kinematics::PlanarChain<2> chain({1.0, 1.0});
    const Eigen::Vector3d pose = chain.fk(Eigen::Vector2d(0.3, -0.2));
    const auto ik = chain.solve_ik(Eigen::Vector2d(0.1, 0.1), Eigen::Vector2d(1.2, 0.5));

    double acc = xi.sum() + u + pose.sum() + quintic.position(1.0) + trap.position(3.0) +
                 kf.state().sum() + spatial::log_so3(R).sum() + T_exp.translation().sum();
    if (K.has_value()) {
        acc += K->sum();
    }
    if (ik.has_value()) {
        acc += ik->sum();
    }
    return acc;
}

} // namespace

// Exported anchor so the exercise code is not eliminated as unused.
[[nodiscard]] double harness_rt_check_anchor();
[[nodiscard]] double harness_rt_check_anchor() {
    return exercise_all();
}
