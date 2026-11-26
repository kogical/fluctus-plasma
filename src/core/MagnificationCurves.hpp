#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace fluctus::core {

enum class MagnificationCurve : std::uint8_t {
    RaisedCosine = 0,       // C^1 finite support (default, macOS-like)
    QuinticSmootherstep = 1, // C^2 finite support
    WindowedGaussian = 2,   // C^0 finite support (WaveTask-like)
};

namespace detail {
inline constexpr double kGaussianK = 4.5;
} // namespace detail

/// Evaluate normalized magnification weight w(u) for u >= 0.
/// Strictly 0 for u >= 1 (finite support), and w(0) == 1.
[[nodiscard]] inline double curveWeight(MagnificationCurve curve, double u) noexcept {
    if (u <= 0.0) {
        return 1.0;
    }
    if (u >= 1.0) {
        return 0.0;
    }

    switch (curve) {
    case MagnificationCurve::RaisedCosine:
        return 0.5 * (1.0 + std::cos(std::numbers::pi * u));

    case MagnificationCurve::QuinticSmootherstep: {
        const double u3 = u * u * u;
        const double s = u3 * (u * (6.0 * u - 15.0) + 10.0);
        return 1.0 - s;
    }

    case MagnificationCurve::WindowedGaussian: {
        const double ek = std::exp(-detail::kGaussianK);
        return (std::exp(-detail::kGaussianK * u * u) - ek) / (1.0 - ek);
    }
    }
    return 0.0;
}

/// Exact closed-form antiderivative W(u) = \int_0^{\min(u, 1)} w(t) dt for u >= 0.
[[nodiscard]] inline double curveIntegral(MagnificationCurve curve, double u) noexcept {
    if (u <= 0.0) {
        return 0.0;
    }
    const double t = std::min(u, 1.0);

    switch (curve) {
    case MagnificationCurve::RaisedCosine:
        return 0.5 * (t + std::sin(std::numbers::pi * t) / std::numbers::pi);

    case MagnificationCurve::QuinticSmootherstep: {
        const double t4 = t * t * t * t;
        const double t5 = t4 * t;
        const double t6 = t5 * t;
        return t - t6 + 3.0 * t5 - 2.5 * t4;
    }

    case MagnificationCurve::WindowedGaussian: {
        const double k = detail::kGaussianK;
        const double sqrtK = std::sqrt(k);
        const double ek = std::exp(-k);
        const double erfTerm = (std::sqrt(std::numbers::pi) / (2.0 * sqrtK)) * std::erf(sqrtK * t);
        return (erfTerm - t * ek) / (1.0 - ek);
    }
    }
    return 0.0;
}

/// Total integral W(1) = \int_0^1 w(t) dt.
[[nodiscard]] inline double curveTotalIntegral(MagnificationCurve curve) noexcept {
    return curveIntegral(curve, 1.0);
}

/// Exact upper bound on primary-axis expansion per side:
/// \Delta_max = (M - 1) * R * W(1).
[[nodiscard]] inline double maxSideSlack(
    MagnificationCurve curve,
    double maxScale,
    double radius
) noexcept {
    if (maxScale <= 1.0 || radius <= 0.0) {
        return 0.0;
    }
    return (maxScale - 1.0) * radius * curveTotalIntegral(curve);
}

} // namespace fluctus::core
