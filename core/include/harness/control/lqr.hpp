#pragma once

#include <Eigen/Cholesky>
#include <Eigen/Core>

#include <optional>

namespace harness::control {

// Infinite-horizon discrete LQR gain via fixed-point iteration of the discrete
// Riccati equation. Returns the gain K for u = -K x, or nullopt if the
// iteration does not converge (e.g. unstabilizable pair).
template <int N, int M>
[[nodiscard]] std::optional<Eigen::Matrix<double, M, N>>
dlqr(const Eigen::Matrix<double, N, N>& A, const Eigen::Matrix<double, N, M>& B,
     const Eigen::Matrix<double, N, N>& Q, const Eigen::Matrix<double, M, M>& R,
     int max_iter = 1000, double tol = 1e-9) {
    Eigen::Matrix<double, N, N> P = Q;
    for (int i = 0; i < max_iter; ++i) {
        const Eigen::Matrix<double, M, M> S = R + B.transpose() * P * B;
        const Eigen::Matrix<double, M, N> K = S.ldlt().solve(B.transpose() * P * A);
        const Eigen::Matrix<double, N, N> P_next = Q + A.transpose() * P * (A - B * K);
        // Divergence (unstabilizable pair) overflows P to inf/NaN; bail out
        // instead of burning the remaining iterations on a comparison that
        // can never become true.
        if (!P_next.allFinite()) {
            return std::nullopt;
        }
        if ((P_next - P).cwiseAbs().maxCoeff() < tol) {
            const Eigen::Matrix<double, M, M> S_final = R + B.transpose() * P_next * B;
            return S_final.ldlt().solve(B.transpose() * P_next * A);
        }
        P = P_next;
    }
    return std::nullopt;
}

} // namespace harness::control
