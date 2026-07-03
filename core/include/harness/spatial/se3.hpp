#pragma once

#include "harness/spatial/so3.hpp"

#include <Eigen/Core>

namespace harness::spatial {

using Vector6d = Eigen::Matrix<double, 6, 1>;

// Rigid transform in 3D. Tangent (twist) convention: xi = [rho; phi] with the
// translational part first, rotational part last.
class SE3 {
public:
    SE3() : R_(Eigen::Matrix3d::Identity()), t_(Eigen::Vector3d::Zero()) {}
    SE3(const Eigen::Matrix3d& R, const Eigen::Vector3d& t) : R_(R), t_(t) {}

    [[nodiscard]] static SE3 Identity() { return SE3{}; }

    [[nodiscard]] const Eigen::Matrix3d& rotation() const { return R_; }
    [[nodiscard]] const Eigen::Vector3d& translation() const { return t_; }

    [[nodiscard]] SE3 operator*(const SE3& other) const {
        return SE3{R_ * other.R_, R_ * other.t_ + t_};
    }

    [[nodiscard]] SE3 inverse() const { return SE3{R_.transpose(), -(R_.transpose() * t_)}; }

    [[nodiscard]] Eigen::Vector3d act(const Eigen::Vector3d& p) const { return R_ * p + t_; }

    [[nodiscard]] Vector6d log() const {
        const Eigen::Vector3d phi = log_so3(R_);
        Vector6d xi;
        xi.head<3>() = left_jacobian_inv_so3(phi) * t_;
        xi.tail<3>() = phi;
        return xi;
    }

private:
    Eigen::Matrix3d R_;
    Eigen::Vector3d t_;
};

[[nodiscard]] inline SE3 exp_se3(const Vector6d& xi) {
    const Eigen::Vector3d rho = xi.head<3>();
    const Eigen::Vector3d phi = xi.tail<3>();
    return SE3{exp_so3(phi), left_jacobian_so3(phi) * rho};
}

} // namespace harness::spatial
