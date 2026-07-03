// cart-pole — LQR balancing of an inverted pendulum on a cart

#include "harness/control/lqr.hpp"

#include <Eigen/Core>

#include <cstdio>

#include "cart_pole_model.hpp"

int main() {
    using namespace cart_pole;
    const Eigen::Matrix4d a = discrete_a();
    const Eigen::Vector4d b = discrete_b();

    const auto k = harness::control::dlqr<4, 1>(a, b, state_cost(), input_cost());
    if (!k.has_value()) {
        std::printf("cart-pole: LQR failed to converge\n");
        return 1;
    }
    const Eigen::Matrix4d a_cl = a - b * (*k);

    Eigen::Vector4d x;
    x << 0.0, 0.0, 0.1, 0.0; // upright +/- 0.1 rad (~5.7 deg) initial tilt

    std::printf("cart-pole: LQR balancing an inverted pendulum on a cart\n");
    std::printf(" t(s)    pos(m)   vel(m/s)   angle(rad)  ang.vel(rad/s)\n");
    constexpr int kSteps = 500; // 10 s at 50 Hz
    for (int i = 0; i <= kSteps; ++i) {
        if (i % 25 == 0) {
            std::printf("%5.2f  %8.4f  %9.4f  %11.4f  %14.4f\n", static_cast<double>(i) * kDt, x(0),
                        x(1), x(2), x(3));
        }
        x = a_cl * x;
    }
    return 0;
}
