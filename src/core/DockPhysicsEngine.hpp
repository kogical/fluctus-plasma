#pragma once

#include "core/AxisGeometry.hpp"
#include "core/BouncePhysics.hpp"
#include "core/DampedSpring.hpp"
#include "core/MagnificationCurves.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace fluctus::core {

struct DockConfig {
    double baseIconSize{48.0};
    double iconSpacing{6.0};
    double maxScale{1.8};
    double influenceRadiusIcons{2.5}; // R = influenceRadiusIcons * (baseIconSize + iconSpacing)
    MagnificationCurve curve{MagnificationCurve::RaisedCosine};
    double hoverSpringOmega{22.0};
    double hoverSpringZeta{1.0};
    double slotSpringOmega{24.0};
    double slotSpringZeta{1.0};
    double bounceRatio{0.55};
    double edgePadding{6.0};
    double pillPaddingPrimary{10.0};
    double pillPaddingCross{6.0};
    double topMargin{6.0};
    double devicePixelRatio{1.0};
    bool bounceEnabled{true};
    bool magnificationEnabled{true};
    bool editModeFrozen{false};
    AxisGeometry axis{};

    [[nodiscard]] double pitch() const noexcept {
        return baseIconSize + iconSpacing;
    }

    [[nodiscard]] double influenceRadius() const noexcept {
        return std::max(1.0, influenceRadiusIcons * pitch());
    }

    [[nodiscard]] double effectiveMaxScale() const noexcept {
        if (!magnificationEnabled || editModeFrozen || maxScale <= 1.0) {
            return 1.0;
        }
        return maxScale;
    }

    [[nodiscard]] double delegateLogicalSize() const noexcept {
        return std::ceil(baseIconSize * std::max(1.0, maxScale));
    }

    [[nodiscard]] double sideSlack() const noexcept {
        return maxSideSlack(curve, std::max(1.0, maxScale), influenceRadius());
    }

    [[nodiscard]] int recommendedPanelThickness() const noexcept {
        return requiredPanelThickness(
            baseIconSize,
            std::max(1.0, maxScale),
            bounceEnabled ? bounceRatio : 0.0,
            edgePadding,
            topMargin
        );
    }
};

struct DockSlotLayout {
    std::uint64_t id{0};
    bool ghost{false};
    double weight{1.0};           // [0, 1] insertion/removal weight
    double restingStart{0.0};     // Canonical resting interval [a_i, b_i]
    double restingEnd{0.0};
    double displayStart{0.0};     // Canonical magnified interval [X(a_i), X(b_i)]
    double displayEnd{0.0};
    double primaryCenter{0.0};    // Container-space primary center
    double scale{1.0};            // Uniform visual scale S_i (>= 1.0)
    double delegateScale{1.0};    // Transform scale for fixed-size delegate: S_i / (delegateSize / s0)
    double bounceOffset{0.0};     // Cross-axis bounce offset (>= 0)
    Vec2 delegateTopLeft{};       // Container-space top-left for setPosition()
    Rect restingRect{};           // Container-space static resting rect (for KWin minimize geometry)
    Rect displayRect{};           // Container-space live visual rect (for hit-testing)
};

struct DockFrame {
    double restingLength{0.0};    // Sum of resting icon widths + gaps
    double implicitPrimary{0.0};  // Resting length + 2 * (pillPaddingPrimary + sideSlack)
    double implicitCross{0.0};    // Recommended panel thickness
    double hoverAlpha{0.0};       // Smoothed engagement factor in [0, 1]
    Rect pillRect{};              // Visible dock pill background rect in container space
    Rect hoverHullRect{};         // Active hover engagement hull in container space
    std::vector<DockSlotLayout> items;
};

/// Pure C++23 macOS dock physics and layout engine.
/// Zero QtQuick dependency — fully testable in headless unit tests.
class DockPhysicsEngine {
public:
    DockPhysicsEngine() = default;

    void setConfig(const DockConfig &config) noexcept;
    [[nodiscard]] const DockConfig &config() const noexcept { return m_config; }

    /// Structural slot management (active slots + ghost slots on removal).
    /// Returns the assigned stable slot ID.
    std::uint64_t insertSlot(std::size_t activeIndex, bool animate = true);
    void removeSlot(std::size_t activeIndex, bool animate = true);
    void moveSlot(std::size_t fromActiveIndex, std::size_t toActiveIndex);
    void clearSlots();

    void setSlotBouncing(std::size_t activeIndex, bool bouncing);

    [[nodiscard]] std::size_t activeSlotCount() const noexcept;
    [[nodiscard]] std::size_t totalSlotCount() const noexcept { return m_slots.size(); }

    /// Pointer / drag interaction in container coordinates.
    void updatePointer(Vec2 containerPoint, double containerWidth, double containerHeight);
    void clearPointer();
    [[nodiscard]] bool isPointerEngaged() const noexcept { return m_pointerEngaged; }

    /// Compute the resting-space insertion index for drag-and-drop (immune to zoom thrashing).
    [[nodiscard]] std::size_t restingInsertionIndex(
        Vec2 containerPoint,
        double containerWidth,
        double containerHeight
    ) const noexcept;

    /// Advance springs and bounce physics by `dt` seconds.
    /// Returns true if any visual state changed or animations are still active.
    bool step(double dt, double containerWidth, double containerHeight);

    /// True when all springs and bounces have settled and the animation loop can stop (0% idle CPU).
    [[nodiscard]] bool isSettled() const noexcept;

    /// Compute the current frame layout for the given container dimensions.
    [[nodiscard]] DockFrame computeFrame(double containerWidth, double containerHeight) const;

    /// Evaluate the continuous resting-space density integral X(r) at canonical coordinate r.
    [[nodiscard]] double mapCanonicalCoordinate(double r, double pointerCanonical, double alpha) const noexcept;

private:
    struct InternalSlot {
        std::uint64_t id{0};
        bool ghost{false};
        DampedSpring weightSpring{};
        BounceState bounce{};
    };

    [[nodiscard]] double computeRestingLength() const noexcept;
    [[nodiscard]] double dockOriginPrimary(double containerPrimary, double restingLength) const noexcept;
    [[nodiscard]] double availableBounceHeadroom(double containerCross) const noexcept;
    [[nodiscard]] std::size_t findActiveInternalIndex(std::size_t activeIndex) const noexcept;

    DockConfig m_config{};
    std::vector<InternalSlot> m_slots;
    std::uint64_t m_nextId{1};

    DampedSpring m_hoverSpring{};
    bool m_pointerEngaged{false};
    double m_pointerCanonical{0.0};
    bool m_hasPointerCanonical{false};
    bool m_dirty{true};
};

} // namespace fluctus::core
