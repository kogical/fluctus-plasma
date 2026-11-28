#pragma once

#include <algorithm>
#include <cmath>

namespace fluctus::core {

/// Latched parabolic gravity bounce for launching / attention-demanding dock icons.
/// - Hop height H is latched at takeoff (u = 0) and clamped to available panel headroom,
///   preventing mid-air jumps when hover magnification changes.
/// - When `active` becomes false, the current hop finishes its descent cleanly (h -> 0).
struct BounceState {
    bool active{false};
    bool inHop{false};
    bool inPause{false};
    double phaseSeconds{0.0};
    double latchedHeight{0.0};
    double currentOffset{0.0};

    static constexpr double kDefaultHopDuration = 0.42;
    static constexpr double kDefaultPauseDuration = 0.14;

    void setActive(bool wantActive) noexcept {
        active = wantActive;
    }

    void reset() noexcept {
        active = false;
        inHop = false;
        inPause = false;
        phaseSeconds = 0.0;
        latchedHeight = 0.0;
        currentOffset = 0.0;
    }

    [[nodiscard]] bool isIdle() const noexcept {
        return !active && !inHop && !inPause && currentOffset <= 0.0;
    }

    /// Advance the bounce state by `dt` seconds.
    /// `desiredHeight` is the ideal hop height (e.g., bounceRatio * baseIconSize * currentScale),
    /// and `availableHeadroom` is the maximum vertical/cross-axis space before clipping the panel.
    void step(
        double dt,
        double desiredHeight,
        double availableHeadroom,
        double hopDuration = kDefaultHopDuration,
        double pauseDuration = kDefaultPauseDuration
    ) noexcept {
        if (dt <= 0.0 || hopDuration <= 0.0) {
            return;
        }

        const double clampedDt = std::min(dt, 1.0 / 30.0);

        if (!inHop && !inPause) {
            if (!active) {
                currentOffset = 0.0;
                return;
            }
            // Start a new hop and latch H at takeoff.
            inHop = true;
            phaseSeconds = 0.0;
            latchedHeight = std::max(0.0, std::min(desiredHeight, availableHeadroom));
        }

        if (inHop) {
            phaseSeconds += clampedDt;
            if (phaseSeconds >= hopDuration) {
                inHop = false;
                currentOffset = 0.0;
                phaseSeconds = 0.0;
                if (active && pauseDuration > 0.0) {
                    inPause = true;
                }
            } else {
                const double u = phaseSeconds / hopDuration;
                currentOffset = 4.0 * latchedHeight * u * (1.0 - u);
            }
            return;
        }

        if (inPause) {
            currentOffset = 0.0;
            if (!active) {
                inPause = false;
                phaseSeconds = 0.0;
                return;
            }
            phaseSeconds += clampedDt;
            if (phaseSeconds >= pauseDuration) {
                inPause = false;
                inHop = true;
                phaseSeconds = 0.0;
                latchedHeight = std::max(0.0, std::min(desiredHeight, availableHeadroom));
            }
        }
    }
};

/// Compute the exact required panel thickness to fit maximum magnification and full bounce
/// without clipping on Wayland:
///   T_required = edgePadding + ceil(s0 * M) + ceil(bounceRatio * s0 * M) + topMargin
[[nodiscard]] inline int requiredPanelThickness(
    double baseIconSize,
    double maxScale,
    double bounceRatio,
    double edgePadding,
    double topMargin
) noexcept {
    const double s0 = std::max(1.0, baseIconSize);
    const double m = std::max(1.0, maxScale);
    const double br = std::max(0.0, bounceRatio);
    const double magnified = std::ceil(s0 * m);
    const double maxBounce = std::ceil(br * s0 * m);
    return static_cast<int>(std::ceil(edgePadding + magnified + maxBounce + topMargin));
}

} // namespace fluctus::core
