#pragma once

#include <cmath>
#include <cstdint>

namespace fluctus::core {

enum class DockEdge : std::uint8_t {
    Bottom = 0,
    Top = 1,
    Left = 2,
    Right = 3,
};

struct Vec2 {
    double x{0.0};
    double y{0.0};
};

struct Rect {
    double x{0.0};
    double y{0.0};
    double width{0.0};
    double height{0.0};
};

struct AxisGeometry {
    DockEdge edge{DockEdge::Bottom};
    bool reversePrimary{false};

    [[nodiscard]] constexpr bool isHorizontal() const noexcept {
        return edge == DockEdge::Bottom || edge == DockEdge::Top;
    }

    [[nodiscard]] constexpr bool isVertical() const noexcept {
        return !isHorizontal();
    }

    /// +1 for normal primary-axis direction, -1 for RTL / reverseMode.
    [[nodiscard]] constexpr double primarySign() const noexcept {
        return reversePrimary ? -1.0 : 1.0;
    }

    /// Cross-axis growth direction away from the screen edge:
    /// Bottom -> -1 (-Y), Top -> +1 (+Y), Left -> +1 (+X), Right -> -1 (-X).
    [[nodiscard]] constexpr double growthSign() const noexcept {
        switch (edge) {
        case DockEdge::Bottom:
        case DockEdge::Right:
            return -1.0;
        case DockEdge::Top:
        case DockEdge::Left:
            return +1.0;
        }
        return -1.0;
    }

    /// Extract the primary-axis coordinate from a 2D container point.
    [[nodiscard]] constexpr double primaryCoord(Vec2 pt) const noexcept {
        return isHorizontal() ? pt.x : pt.y;
    }

    /// Extract the cross-axis coordinate from a 2D container point.
    [[nodiscard]] constexpr double crossCoord(Vec2 pt) const noexcept {
        return isHorizontal() ? pt.y : pt.x;
    }

    /// Extract primary-axis extent from a container size.
    [[nodiscard]] constexpr double primaryExtent(double width, double height) const noexcept {
        return isHorizontal() ? width : height;
    }

    /// Extract cross-axis extent from a container size.
    [[nodiscard]] constexpr double crossExtent(double width, double height) const noexcept {
        return isHorizontal() ? height : width;
    }

    /// Convert a container-space primary coordinate into canonical resting space [0, totalLength],
    /// accounting for primary centering offset and RTL/reverse direction.
    [[nodiscard]] constexpr double containerToCanonicalPrimary(
        double containerPrimary,
        double dockOriginPrimary,
        double restingLength
    ) const noexcept {
        const double rel = containerPrimary - dockOriginPrimary;
        return reversePrimary ? (restingLength - rel) : rel;
    }

    /// Convert a canonical primary coordinate back into container space.
    [[nodiscard]] constexpr double canonicalToContainerPrimary(
        double canonicalPrimary,
        double dockOriginPrimary,
        double restingLength
    ) const noexcept {
        const double rel = reversePrimary ? (restingLength - canonicalPrimary) : canonicalPrimary;
        return dockOriginPrimary + rel;
    }

    /// Compute the top-left position of an unscaled delegate of fixed logical size `delegateSize`
    /// whose transformOrigin is on the screen-edge side (Item.Bottom / Top / Left / Right),
    /// so that scaling by `scaleFactor` places its center at `primaryCenter` and its edge-anchored
    /// base at `edgePadding + bounceOffset` away from the screen edge.
    [[nodiscard]] constexpr Vec2 delegateTopLeft(
        double primaryCenter,
        double delegateSize,
        double containerWidth,
        double containerHeight,
        double edgePadding,
        double bounceOffset
    ) const noexcept {
        const double halfSize = delegateSize * 0.5;
        const double p = primaryCenter - halfSize;

        switch (edge) {
        case DockEdge::Bottom:
            return Vec2{p, containerHeight - edgePadding - bounceOffset - delegateSize};
        case DockEdge::Top:
            return Vec2{p, edgePadding + bounceOffset};
        case DockEdge::Left:
            return Vec2{edgePadding + bounceOffset, p};
        case DockEdge::Right:
            return Vec2{containerWidth - edgePadding - bounceOffset - delegateSize, p};
        }
        return Vec2{p, containerHeight - edgePadding - bounceOffset - delegateSize};
    }

    /// Construct a 2D rectangle from primary interval [pStart, pEnd] and cross distance
    /// [crossStart, crossStart + crossThickness] measured inward from the screen edge.
    [[nodiscard]] constexpr Rect makeEdgeRect(
        double pStart,
        double pEnd,
        double crossStart,
        double crossThickness,
        double containerWidth,
        double containerHeight
    ) const noexcept {
        const double pMin = pStart < pEnd ? pStart : pEnd;
        const double pLen = std::abs(pEnd - pStart);

        switch (edge) {
        case DockEdge::Bottom:
            return Rect{pMin, containerHeight - crossStart - crossThickness, pLen, crossThickness};
        case DockEdge::Top:
            return Rect{pMin, crossStart, pLen, crossThickness};
        case DockEdge::Left:
            return Rect{crossStart, pMin, crossThickness, pLen};
        case DockEdge::Right:
            return Rect{containerWidth - crossStart - crossThickness, pMin, crossThickness, pLen};
        }
        return Rect{pMin, containerHeight - crossStart - crossThickness, pLen, crossThickness};
    }
};

} // namespace fluctus::core
