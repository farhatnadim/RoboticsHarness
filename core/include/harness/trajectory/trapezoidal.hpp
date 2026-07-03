#pragma once

#include <algorithm>
#include <cmath>

namespace harness::trajectory {

// Rest-to-rest trapezoidal velocity profile over a signed distance, degrading
// to a triangular profile when v_max cannot be reached.
// Preconditions: v_max > 0, a_max > 0. Queries outside [0, duration()] clamp.
class TrapezoidalProfile {
public:
    TrapezoidalProfile(double distance, double v_max, double a_max)
        : sign_(distance < 0.0 ? -1.0 : 1.0), accel_(a_max) {
        const double d = std::abs(distance);
        const double t_full_acc = v_max / a_max;
        const double d_full_acc = 0.5 * v_max * t_full_acc;
        if (2.0 * d_full_acc >= d) {
            t_acc_ = std::sqrt(d / a_max);
            v_peak_ = a_max * t_acc_;
            t_cruise_ = 0.0;
        } else {
            t_acc_ = t_full_acc;
            v_peak_ = v_max;
            t_cruise_ = (d - 2.0 * d_full_acc) / v_max;
        }
        distance_ = accel_ * t_acc_ * t_acc_ + v_peak_ * t_cruise_;
    }

    [[nodiscard]] double duration() const { return 2.0 * t_acc_ + t_cruise_; }

    [[nodiscard]] double position(double t) const {
        const double tc = std::clamp(t, 0.0, duration());
        double p = 0.0;
        if (tc < t_acc_) {
            p = 0.5 * accel_ * tc * tc;
        } else if (tc < t_acc_ + t_cruise_) {
            p = 0.5 * accel_ * t_acc_ * t_acc_ + v_peak_ * (tc - t_acc_);
        } else {
            const double remaining = duration() - tc;
            p = distance_ - 0.5 * accel_ * remaining * remaining;
        }
        return sign_ * p;
    }

    [[nodiscard]] double velocity(double t) const {
        const double tc = std::clamp(t, 0.0, duration());
        double v = 0.0;
        if (tc < t_acc_) {
            v = accel_ * tc;
        } else if (tc < t_acc_ + t_cruise_) {
            v = v_peak_;
        } else {
            v = accel_ * (duration() - tc);
        }
        return sign_ * v;
    }

private:
    double sign_;
    double accel_;
    double t_acc_{0.0};
    double t_cruise_{0.0};
    double v_peak_{0.0};
    double distance_{0.0};
};

} // namespace harness::trajectory
