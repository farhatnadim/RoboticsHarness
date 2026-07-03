#pragma once

#include <Eigen/Core>
#include <Eigen/LU>

#include <array>
#include <cmath>
#include <cstddef>
#include <optional>

namespace harness::kinematics {

// Planar serial chain of N revolute joints with fixed link lengths.
// fk() returns (x, y, theta) of the end effector; solve_ik() is
// position-only damped-least-squares with bounded iterations.
template <int N>
class PlanarChain {
    static_assert(N >= 1, "chain needs at least one joint");

public:
    using JointVec = Eigen::Matrix<double, N, 1>;

    explicit PlanarChain(const std::array<double, static_cast<std::size_t>(N)>& link_lengths)
        : lengths_(link_lengths) {}

    [[nodiscard]] Eigen::Vector3d fk(const JointVec& q) const {
        double x = 0.0;
        double y = 0.0;
        double cum = 0.0;
        for (int i = 0; i < N; ++i) {
            cum += q(i);
            const double len = lengths_[static_cast<std::size_t>(i)];
            x += len * std::cos(cum);
            y += len * std::sin(cum);
        }
        return {x, y, cum};
    }

    [[nodiscard]] Eigen::Matrix<double, 3, N> jacobian(const JointVec& q) const {
        std::array<double, static_cast<std::size_t>(N)> cum{};
        double c = 0.0;
        for (int i = 0; i < N; ++i) {
            c += q(i);
            cum[static_cast<std::size_t>(i)] = c;
        }
        Eigen::Matrix<double, 3, N> J;
        for (int j = 0; j < N; ++j) {
            double jx = 0.0;
            double jy = 0.0;
            for (int i = j; i < N; ++i) {
                const auto ui = static_cast<std::size_t>(i);
                jx -= lengths_[ui] * std::sin(cum[ui]);
                jy += lengths_[ui] * std::cos(cum[ui]);
            }
            J(0, j) = jx;
            J(1, j) = jy;
            J(2, j) = 1.0;
        }
        return J;
    }

    [[nodiscard]] std::optional<JointVec> solve_ik(const JointVec& q0,
                                                   const Eigen::Vector2d& target,
                                                   double damping = 0.1, int max_iter = 200,
                                                   double tol = 1e-6) const {
        JointVec q = q0;
        for (int it = 0; it < max_iter; ++it) {
            const Eigen::Vector2d err = target - fk(q).template head<2>();
            if (err.norm() < tol) {
                return q;
            }
            const Eigen::Matrix<double, 2, N> Jp = jacobian(q).template topRows<2>();
            const Eigen::Matrix2d JJt =
                Jp * Jp.transpose() + damping * damping * Eigen::Matrix2d::Identity();
            q += Jp.transpose() * JJt.inverse() * err;
        }
        return std::nullopt;
    }

private:
    std::array<double, static_cast<std::size_t>(N)> lengths_;
};

} // namespace harness::kinematics
