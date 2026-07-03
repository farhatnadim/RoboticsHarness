#pragma once

#include <Eigen/Cholesky>
#include <Eigen/Core>

namespace harness::estimation {

// Linear Kalman filter over fixed-size state (NX) and measurement (NZ).
// Uses the Joseph-form covariance update to preserve symmetry and positive
// semi-definiteness under roundoff.
template <int NX, int NZ>
class KalmanFilter {
public:
    using StateVec = Eigen::Matrix<double, NX, 1>;
    using StateMat = Eigen::Matrix<double, NX, NX>;
    using MeasVec = Eigen::Matrix<double, NZ, 1>;
    using MeasMat = Eigen::Matrix<double, NZ, NZ>;
    using MeasModel = Eigen::Matrix<double, NZ, NX>;

    KalmanFilter(const StateMat& A, const MeasModel& H, const StateMat& Q, const MeasMat& R,
                 const StateVec& x0, const StateMat& P0)
        : A_(A), H_(H), Q_(Q), R_(R), x_(x0), P_(P0) {}

    void predict() {
        x_ = A_ * x_;
        P_ = A_ * P_ * A_.transpose() + Q_;
    }

    void update(const MeasVec& z) {
        const MeasVec innovation = z - H_ * x_;
        const MeasMat S = H_ * P_ * H_.transpose() + R_;
        const Eigen::Matrix<double, NX, NZ> K = S.ldlt().solve(H_ * P_).transpose();
        x_ += K * innovation;
        const StateMat I_KH = StateMat::Identity() - K * H_;
        P_ = I_KH * P_ * I_KH.transpose() + K * R_ * K.transpose();
    }

    [[nodiscard]] const StateVec& state() const { return x_; }
    [[nodiscard]] const StateMat& covariance() const { return P_; }

private:
    StateMat A_;
    MeasModel H_;
    StateMat Q_;
    MeasMat R_;
    StateVec x_;
    StateMat P_;
};

} // namespace harness::estimation
