#pragma once

#include <algorithm>
#include <cmath>

namespace fluctus::core {

/// Numerically safe analytical 1D damped harmonic oscillator:
///   y'' = omega^2 * (target - y) - 2 * zeta * omega * y'
/// Handles critically damped (|zeta - 1| < 1e-4), underdamped, and overdamped regimes
/// without catastrophic cancellation.
struct DampedSpring {
    double value{0.0};
    double velocity{0.0};
    double target{0.0};

    static constexpr double kMaxStepSeconds = 1.0 / 30.0;
    static constexpr double kCriticalEpsilon = 1e-4;

    void snap(double v) noexcept {
        value = v;
        target = v;
        velocity = 0.0;
    }

    /// Advance the spring by `dt` seconds with natural frequency `omega` (rad/s)
    /// and damping ratio `zeta` (>= 0).
    void step(double dt, double omega, double zeta) noexcept {
        if (dt <= 0.0 || omega <= 0.0) {
            return;
        }
        const double h = std::min(dt, kMaxStepSeconds);
        const double y0 = value - target;
        const double v0 = velocity;
        const double z = std::max(zeta, 0.0);

        double y1 = 0.0;
        double v1 = 0.0;

        if (std::abs(z - 1.0) < kCriticalEpsilon) {
            // Critically damped: e^(-w*h) * (y0 + (v0 + w*y0)*h)
            const double decay = std::exp(-omega * h);
            const double c1 = y0;
            const double c2 = v0 + omega * y0;
            y1 = decay * (c1 + c2 * h);
            v1 = decay * (c2 - omega * (c1 + c2 * h));
        } else if (z < 1.0) {
            // Underdamped
            const double wd = omega * std::sqrt(1.0 - z * z);
            const double decay = std::exp(-z * omega * h);
            const double cosTerm = std::cos(wd * h);
            const double sinTerm = std::sin(wd * h);
            const double c1 = y0;
            const double c2 = (v0 + z * omega * y0) / wd;
            y1 = decay * (c1 * cosTerm + c2 * sinTerm);
            v1 = decay * ((c2 * wd - z * omega * c1) * cosTerm
                          - (c1 * wd + z * omega * c2) * sinTerm);
        } else {
            // Overdamped
            const double disc = std::sqrt(z * z - 1.0);
            const double r1 = -omega * (z - disc);
            const double r2 = -omega * (z + disc);
            const double invDiff = 1.0 / (r1 - r2);
            const double c1 = (v0 - r2 * y0) * invDiff;
            const double c2 = (r1 * y0 - v0) * invDiff;
            const double e1 = std::exp(r1 * h);
            const double e2 = std::exp(r2 * h);
            y1 = c1 * e1 + c2 * e2;
            v1 = c1 * r1 * e1 + c2 * r2 * e2;
        }

        value = target + y1;
        velocity = v1;
    }

    /// Check whether the spring has settled within `posEpsilon` and `velEpsilon`.
    [[nodiscard]] bool isSettled(double posEpsilon, double velEpsilon) const noexcept {
        return std::abs(value - target) <= posEpsilon && std::abs(velocity) <= velEpsilon;
    }

    /// Snap to target if settled; returns true if settled.
    bool settleIfClose(double posEpsilon, double velEpsilon) noexcept {
        if (isSettled(posEpsilon, velEpsilon)) {
            value = target;
            velocity = 0.0;
            return true;
        }
        return false;
    }
};

} // namespace fluctus::core
