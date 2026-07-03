#pragma once

#include <Eigen/Core>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace harness::spatial {

// Skew-symmetric matrix of w, such that hat(w) * v == w.cross(v).
[[nodiscard]] inline Eigen::Matrix3d hat(const Eigen::Vector3d& w) {
    Eigen::Matrix3d m;
    m << 0.0, -w.z(), w.y(), w.z(), 0.0, -w.x(), -w.y(), w.x(), 0.0;
    return m;
}

// Inverse of hat: extracts the vector from a skew-symmetric matrix.
[[nodiscard]] inline Eigen::Vector3d vee(const Eigen::Matrix3d& m) {
    return {m(2, 1), m(0, 2), m(1, 0)};
}

// Rodrigues formula with a series branch below this angle.
inline constexpr double kSmallAngle = 1e-8;

[[nodiscard]] inline Eigen::Matrix3d exp_so3(const Eigen::Vector3d& w) {
    const double theta = w.norm();
    const Eigen::Matrix3d W = hat(w);
    if (theta < kSmallAngle) {
        return Eigen::Matrix3d::Identity() + W + 0.5 * W * W;
    }
    const double a = std::sin(theta) / theta;
    const double b = (1.0 - std::cos(theta)) / (theta * theta);
    return Eigen::Matrix3d::Identity() + a * W + b * W * W;
}

// Logarithm of a rotation matrix. Near theta == pi the axis is recovered from
// the symmetric part (R + I)/2 == w w^T, with the sign disambiguated by the
// (vanishing) skew part; exp(pi*w) == exp(-pi*w), so either sign is a valid log.
[[nodiscard]] inline Eigen::Vector3d log_so3(const Eigen::Matrix3d& R) {
    const double cos_theta = std::clamp(0.5 * (R.trace() - 1.0), -1.0, 1.0);
    const double theta = std::acos(cos_theta);
    if (theta < kSmallAngle) {
        return 0.5 * vee(R - R.transpose());
    }
    if (theta > std::numbers::pi - 1e-6) {
        const Eigen::Matrix3d S = 0.5 * (R + Eigen::Matrix3d::Identity());
        Eigen::Index k = 0;
        S.diagonal().maxCoeff(&k);
        Eigen::Vector3d axis = S.col(k) / std::sqrt(S(k, k));
        axis.normalize();
        if (vee(R - R.transpose()).dot(axis) < 0.0) {
            axis = -axis;
        }
        return theta * axis;
    }
    return (theta / (2.0 * std::sin(theta))) * vee(R - R.transpose());
}

// Left Jacobian of SO(3): translates tangent-space increments to group motion,
// used by the SE(3) exponential.
[[nodiscard]] inline Eigen::Matrix3d left_jacobian_so3(const Eigen::Vector3d& phi) {
    const double theta = phi.norm();
    const Eigen::Matrix3d W = hat(phi);
    if (theta < kSmallAngle) {
        return Eigen::Matrix3d::Identity() + 0.5 * W + W * W / 6.0;
    }
    const double t2 = theta * theta;
    const double a = (1.0 - std::cos(theta)) / t2;
    const double b = (theta - std::sin(theta)) / (t2 * theta);
    return Eigen::Matrix3d::Identity() + a * W + b * W * W;
}

[[nodiscard]] inline Eigen::Matrix3d left_jacobian_inv_so3(const Eigen::Vector3d& phi) {
    const double theta = phi.norm();
    const Eigen::Matrix3d W = hat(phi);
    if (theta < kSmallAngle) {
        return Eigen::Matrix3d::Identity() - 0.5 * W + W * W / 12.0;
    }
    const double half = 0.5 * theta;
    const double coeff = (1.0 - half * std::cos(half) / std::sin(half)) / (theta * theta);
    return Eigen::Matrix3d::Identity() - 0.5 * W + coeff * W * W;
}

} // namespace harness::spatial
