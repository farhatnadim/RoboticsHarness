#pragma once

#include <Eigen/Core>
#include <Eigen/LU>

#include <array>

namespace harness::trajectory {

// Quintic polynomial connecting full boundary states (position, velocity,
// acceleration) at t = 0 and t = duration. Precondition: duration > 0.
// Evaluation outside [0, duration] extrapolates the polynomial.
class QuinticPolynomial {
public:
    QuinticPolynomial(double p0, double v0, double a0, double pf, double vf, double af,
                      double duration)
        : duration_(duration) {
        coeffs_[0] = p0;
        coeffs_[1] = v0;
        coeffs_[2] = 0.5 * a0;
        const double T = duration;
        const double T2 = T * T;
        const double T3 = T2 * T;
        const double T4 = T3 * T;
        const double T5 = T4 * T;
        Eigen::Matrix3d M;
        M << T3, T4, T5, 3.0 * T2, 4.0 * T3, 5.0 * T4, 6.0 * T, 12.0 * T2, 20.0 * T3;
        const Eigen::Vector3d rhs(pf - (p0 + v0 * T + 0.5 * a0 * T2), vf - (v0 + a0 * T),
                                  af - a0);
        const Eigen::Vector3d high = M.inverse() * rhs;
        coeffs_[3] = high(0);
        coeffs_[4] = high(1);
        coeffs_[5] = high(2);
    }

    [[nodiscard]] double position(double t) const {
        return coeffs_[0] +
               t * (coeffs_[1] +
                    t * (coeffs_[2] + t * (coeffs_[3] + t * (coeffs_[4] + t * coeffs_[5]))));
    }

    [[nodiscard]] double velocity(double t) const {
        return coeffs_[1] +
               t * (2.0 * coeffs_[2] +
                    t * (3.0 * coeffs_[3] + t * (4.0 * coeffs_[4] + t * 5.0 * coeffs_[5])));
    }

    [[nodiscard]] double acceleration(double t) const {
        return 2.0 * coeffs_[2] +
               t * (6.0 * coeffs_[3] + t * (12.0 * coeffs_[4] + t * 20.0 * coeffs_[5]));
    }

    [[nodiscard]] double duration() const { return duration_; }

private:
    std::array<double, 6> coeffs_{};
    double duration_;
};

} // namespace harness::trajectory
