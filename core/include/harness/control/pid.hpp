#pragma once

#include <algorithm>
#include <limits>

namespace harness::control {

// Discrete PID with clamped integral (anti-windup) and derivative-on-error.
// The first update after construction or reset() uses a zero derivative.
class Pid {
public:
    struct Gains {
        double kp{0.0};
        double ki{0.0};
        double kd{0.0};
    };

    struct Limits {
        double out_min{-std::numeric_limits<double>::infinity()};
        double out_max{std::numeric_limits<double>::infinity()};
        double i_min{-std::numeric_limits<double>::infinity()};
        double i_max{std::numeric_limits<double>::infinity()};
    };

    explicit Pid(const Gains& gains) : Pid(gains, Limits{}) {}
    Pid(const Gains& gains, const Limits& limits) : gains_(gains), limits_(limits) {}

    // dt <= 0 degrades to a pure clamped P response and leaves state untouched.
    [[nodiscard]] double update(double error, double dt) {
        if (dt <= 0.0) {
            return std::clamp(gains_.kp * error, limits_.out_min, limits_.out_max);
        }
        integral_ = std::clamp(integral_ + error * dt, limits_.i_min, limits_.i_max);
        const double derivative = has_prev_ ? (error - prev_error_) / dt : 0.0;
        prev_error_ = error;
        has_prev_ = true;
        const double out = gains_.kp * error + gains_.ki * integral_ + gains_.kd * derivative;
        return std::clamp(out, limits_.out_min, limits_.out_max);
    }

    void reset() {
        integral_ = 0.0;
        prev_error_ = 0.0;
        has_prev_ = false;
    }

    [[nodiscard]] double integral() const { return integral_; }

private:
    Gains gains_;
    Limits limits_;
    double integral_{0.0};
    double prev_error_{0.0};
    bool has_prev_{false};
};

} // namespace harness::control
