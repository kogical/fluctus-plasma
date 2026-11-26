#include "core/DockPhysicsEngine.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace fluctus::core {

namespace {

[[nodiscard]] double snapToDevicePixel(double logicalValue, double dpr) noexcept {
    if (dpr <= 0.0) {
        return logicalValue;
    }
    return std::round(logicalValue * dpr) / dpr;
}

} // namespace

void DockPhysicsEngine::setConfig(const DockConfig &config) noexcept {
    m_config = config;
    if (m_config.editModeFrozen || !m_config.magnificationEnabled) {
        m_hoverSpring.target = 0.0;
        m_pointerEngaged = false;
    }
    m_dirty = true;
}

std::uint64_t DockPhysicsEngine::insertSlot(std::size_t activeIndex, bool animate) {
    InternalSlot slot{};
    slot.id = m_nextId++;
    slot.ghost = false;
    if (animate) {
        slot.weightSpring.value = 0.0;
        slot.weightSpring.velocity = 0.0;
        slot.weightSpring.target = 1.0;
    } else {
        slot.weightSpring.snap(1.0);
    }

    // Find insertion position in m_slots corresponding to activeIndex.
    std::size_t activeSeen = 0;
    std::size_t insertPos = m_slots.size();
    for (std::size_t i = 0; i < m_slots.size(); ++i) {
        if (!m_slots[i].ghost) {
            if (activeSeen == activeIndex) {
                insertPos = i;
                break;
            }
            ++activeSeen;
        }
    }

    m_slots.insert(m_slots.begin() + static_cast<std::ptrdiff_t>(insertPos), slot);
    m_dirty = true;
    return slot.id;
}

void DockPhysicsEngine::removeSlot(std::size_t activeIndex, bool animate) {
    const std::size_t idx = findActiveInternalIndex(activeIndex);
    if (idx >= m_slots.size()) {
        return;
    }

    if (!animate) {
        m_slots.erase(m_slots.begin() + static_cast<std::ptrdiff_t>(idx));
    } else {
        m_slots[idx].ghost = true;
        m_slots[idx].weightSpring.target = 0.0;
        m_slots[idx].bounce.setActive(false);
    }
    m_dirty = true;
}

void DockPhysicsEngine::moveSlot(std::size_t fromActiveIndex, std::size_t toActiveIndex) {
    if (fromActiveIndex == toActiveIndex) {
        return;
    }
    const std::size_t fromIdx = findActiveInternalIndex(fromActiveIndex);
    if (fromIdx >= m_slots.size()) {
        return;
    }

    InternalSlot moved = m_slots[fromIdx];
    m_slots.erase(m_slots.begin() + static_cast<std::ptrdiff_t>(fromIdx));

    std::size_t activeSeen = 0;
    std::size_t insertPos = m_slots.size();
    for (std::size_t i = 0; i < m_slots.size(); ++i) {
        if (!m_slots[i].ghost) {
            if (activeSeen == toActiveIndex) {
                insertPos = i;
                break;
            }
            ++activeSeen;
        }
    }

    m_slots.insert(m_slots.begin() + static_cast<std::ptrdiff_t>(insertPos), moved);
    m_dirty = true;
}

void DockPhysicsEngine::clearSlots() {
    m_slots.clear();
    m_pointerEngaged = false;
    m_hoverSpring.snap(0.0);
    m_dirty = true;
}

void DockPhysicsEngine::setSlotBouncing(std::size_t activeIndex, bool bouncing) {
    const std::size_t idx = findActiveInternalIndex(activeIndex);
    if (idx >= m_slots.size()) {
        return;
    }
    m_slots[idx].bounce.setActive(bouncing && m_config.bounceEnabled);
    m_dirty = true;
}

std::size_t DockPhysicsEngine::activeSlotCount() const noexcept {
    std::size_t count = 0;
    for (const auto &slot : m_slots) {
        if (!slot.ghost) {
            ++count;
        }
    }
    return count;
}

double DockPhysicsEngine::computeRestingLength() const noexcept {
    if (m_slots.empty()) {
        return 0.0;
    }
    double totalWeight = 0.0;
    for (const auto &slot : m_slots) {
        totalWeight += std::clamp(slot.weightSpring.value, 0.0, 1.0);
    }
    if (totalWeight <= 0.0) {
        return 0.0;
    }
    const double iconSum = totalWeight * m_config.baseIconSize;
    const double gapCount = std::max(0.0, totalWeight - 1.0);
    return iconSum + gapCount * m_config.iconSpacing;
}

double DockPhysicsEngine::dockOriginPrimary(
    double containerPrimary,
    double restingLength
) const noexcept {
    return (containerPrimary - restingLength) * 0.5;
}

double DockPhysicsEngine::availableBounceHeadroom(double containerCross) const noexcept {
    const double magnifiedIcon = std::ceil(m_config.baseIconSize * m_config.effectiveMaxScale());
    const double used = m_config.edgePadding + magnifiedIcon + m_config.topMargin;
    return std::max(0.0, containerCross - used);
}

std::size_t DockPhysicsEngine::findActiveInternalIndex(std::size_t activeIndex) const noexcept {
    std::size_t activeSeen = 0;
    for (std::size_t i = 0; i < m_slots.size(); ++i) {
        if (!m_slots[i].ghost) {
            if (activeSeen == activeIndex) {
                return i;
            }
            ++activeSeen;
        }
    }
    return m_slots.size();
}

void DockPhysicsEngine::updatePointer(
    Vec2 containerPoint,
    double containerWidth,
    double containerHeight
) {
    if (m_config.editModeFrozen || !m_config.magnificationEnabled || m_slots.empty()) {
        clearPointer();
        return;
    }

    const DockFrame current = computeFrame(containerWidth, containerHeight);
    const Rect &hull = current.hoverHullRect;
    const bool insideHull = containerPoint.x >= hull.x
        && containerPoint.x <= hull.x + hull.width
        && containerPoint.y >= hull.y
        && containerPoint.y <= hull.y + hull.height;

    if (!insideHull) {
        clearPointer();
        return;
    }

    const double restingLength = current.restingLength;
    const double containerPrimary = m_config.axis.primaryExtent(containerWidth, containerHeight);
    const double originP = dockOriginPrimary(containerPrimary, restingLength);
    const double pContainer = m_config.axis.primaryCoord(containerPoint);
    const double pCanonical = m_config.axis.containerToCanonicalPrimary(pContainer, originP, restingLength);

    m_pointerCanonical = std::clamp(pCanonical, 0.0, std::max(0.0, restingLength));
    m_hasPointerCanonical = true;
    m_pointerEngaged = true;
    m_hoverSpring.target = 1.0;
    m_dirty = true;
}

void DockPhysicsEngine::clearPointer() {
    if (m_pointerEngaged || m_hoverSpring.target != 0.0) {
        m_pointerEngaged = false;
        // Freeze m_pointerCanonical so the dock collapses cleanly without snapping sideways.
        m_hoverSpring.target = 0.0;
        m_dirty = true;
    }
}

std::size_t DockPhysicsEngine::restingInsertionIndex(
    Vec2 containerPoint,
    double containerWidth,
    double containerHeight
) const noexcept {
    const std::size_t count = activeSlotCount();
    if (count == 0) {
        return 0;
    }

    const double restingLength = static_cast<double>(count) * m_config.baseIconSize
        + static_cast<double>(count - 1) * m_config.iconSpacing;
    const double containerPrimary = m_config.axis.primaryExtent(containerWidth, containerHeight);
    const double originP = dockOriginPrimary(containerPrimary, restingLength);
    const double pContainer = m_config.axis.primaryCoord(containerPoint);
    const double pCanonical = m_config.axis.containerToCanonicalPrimary(pContainer, originP, restingLength);

    if (pCanonical <= 0.0) {
        return 0;
    }
    const double pitch = m_config.pitch();
    if (pitch <= 0.0) {
        return 0;
    }
    const double rawIndex = std::floor((pCanonical + m_config.iconSpacing * 0.5) / pitch);
    return static_cast<std::size_t>(std::clamp(rawIndex, 0.0, static_cast<double>(count)));
}

double DockPhysicsEngine::mapCanonicalCoordinate(
    double r,
    double pointerCanonical,
    double alpha
) const noexcept {
    const double maxScale = m_config.effectiveMaxScale();
    if (maxScale <= 1.0 || alpha <= 0.0) {
        return r;
    }
    const double radius = m_config.influenceRadius();
    const double diff = r - pointerCanonical;
    const double dist = std::abs(diff);
    if (dist <= 0.0) {
        return r;
    }
    const double sign = diff >= 0.0 ? 1.0 : -1.0;
    const double u = std::min(dist / radius, 1.0);
    const double shift = sign * (maxScale - 1.0) * alpha * radius * curveIntegral(m_config.curve, u);
    return r + shift;
}

bool DockPhysicsEngine::step(double dt, double containerWidth, double containerHeight) {
    const double dpr = std::max(1.0, m_config.devicePixelRatio);
    const double scaleSpan = std::max(1.0, m_config.baseIconSize * (m_config.effectiveMaxScale() - 1.0));
    const double alphaPosEps = 0.25 / (scaleSpan * dpr);
    const double alphaVelEps = alphaPosEps * 4.0;
    const double weightPosEps = 0.25 / (std::max(1.0, m_config.baseIconSize) * dpr);
    const double weightVelEps = weightPosEps * 4.0;

    // 1. Step hover engagement spring (strictly clamped to [0, 1] to prevent negative-scale overshoot).
    m_hoverSpring.step(dt, m_config.hoverSpringOmega, std::max(1.0, m_config.hoverSpringZeta));
    m_hoverSpring.value = std::clamp(m_hoverSpring.value, 0.0, 1.0);
    m_hoverSpring.settleIfClose(alphaPosEps, alphaVelEps);

    // 2. Step slot weight springs and prune finished ghost slots.
    for (auto &slot : m_slots) {
        slot.weightSpring.step(dt, m_config.slotSpringOmega, std::max(1.0, m_config.slotSpringZeta));
        slot.weightSpring.value = std::clamp(slot.weightSpring.value, 0.0, 1.0);
        slot.weightSpring.settleIfClose(weightPosEps, weightVelEps);
    }

    std::erase_if(m_slots, [](const InternalSlot &slot) {
        return slot.ghost && slot.weightSpring.value <= 0.0 && slot.weightSpring.target <= 0.0;
    });

    // 3. Step latched bounce physics using current scale and available panel headroom.
    const double containerCross = m_config.axis.crossExtent(containerWidth, containerHeight);
    const double headroom = availableBounceHeadroom(containerCross);
    const double alpha = m_hoverSpring.value;
    const double restingLength = computeRestingLength();
    const double pointer = m_hasPointerCanonical ? m_pointerCanonical : restingLength * 0.5;

    double cursor = 0.0;
    for (std::size_t i = 0; i < m_slots.size(); ++i) {
        auto &slot = m_slots[i];
        const double w = std::clamp(slot.weightSpring.value, 0.0, 1.0);
        const double a = cursor;
        const double b = cursor + m_config.baseIconSize * w;
        double slotScale = 1.0;
        if (w > 1e-6) {
            const double xa = mapCanonicalCoordinate(a, pointer, alpha);
            const double xb = mapCanonicalCoordinate(b, pointer, alpha);
            slotScale = std::max(1.0, (xb - xa) / (m_config.baseIconSize * w));
        }
        const double desiredHop = m_config.bounceEnabled
            ? (m_config.bounceRatio * m_config.baseIconSize * slotScale)
            : 0.0;
        slot.bounce.step(dt, desiredHop, headroom);
        cursor = b + m_config.iconSpacing * w;
    }

    const bool settled = isSettled();
    const bool changed = m_dirty || !settled;
    m_dirty = false;
    return changed;
}

bool DockPhysicsEngine::isSettled() const noexcept {
    if (m_dirty) {
        return false;
    }
    const double dpr = std::max(1.0, m_config.devicePixelRatio);
    const double scaleSpan = std::max(1.0, m_config.baseIconSize * (m_config.effectiveMaxScale() - 1.0));
    const double alphaPosEps = 0.25 / (scaleSpan * dpr);
    const double alphaVelEps = alphaPosEps * 4.0;
    if (!m_hoverSpring.isSettled(alphaPosEps, alphaVelEps)) {
        return false;
    }

    const double weightPosEps = 0.25 / (std::max(1.0, m_config.baseIconSize) * dpr);
    const double weightVelEps = weightPosEps * 4.0;
    for (const auto &slot : m_slots) {
        if (slot.ghost) {
            return false;
        }
        if (!slot.weightSpring.isSettled(weightPosEps, weightVelEps)) {
            return false;
        }
        if (!slot.bounce.isIdle()) {
            return false;
        }
    }
    return true;
}

DockFrame DockPhysicsEngine::computeFrame(double containerWidth, double containerHeight) const {
    DockFrame frame{};
    const double restingLength = computeRestingLength();
    const double sideSlack = m_config.sideSlack();
    const double alpha = std::clamp(m_hoverSpring.value, 0.0, 1.0);
    const double dpr = std::max(1.0, m_config.devicePixelRatio);
    const bool settledAtRest = (alpha <= 0.0) && isSettled();

    frame.restingLength = restingLength;
    frame.implicitPrimary = restingLength + 2.0 * (m_config.pillPaddingPrimary + sideSlack);
    frame.implicitCross = static_cast<double>(m_config.recommendedPanelThickness());
    frame.hoverAlpha = alpha;

    const double containerPrimary = m_config.axis.primaryExtent(containerWidth, containerHeight);
    const double originP = dockOriginPrimary(containerPrimary, restingLength);
    const double pointer = m_hasPointerCanonical ? m_pointerCanonical : restingLength * 0.5;
    const double delegateSize = m_config.delegateLogicalSize();
    const double invDelegateRatio = m_config.baseIconSize / std::max(1.0, delegateSize);

    frame.items.reserve(m_slots.size());

    double minDisplayP = std::numeric_limits<double>::infinity();
    double maxDisplayP = -std::numeric_limits<double>::infinity();
    double maxIconCrossExtent = m_config.baseIconSize;

    double cursor = 0.0;
    for (std::size_t i = 0; i < m_slots.size(); ++i) {
        const auto &slot = m_slots[i];
        const double w = std::clamp(slot.weightSpring.value, 0.0, 1.0);
        const double a = cursor;
        const double b = cursor + m_config.baseIconSize * w;
        const double xa = mapCanonicalCoordinate(a, pointer, alpha);
        const double xb = mapCanonicalCoordinate(b, pointer, alpha);

        const double span = std::max(0.0, xb - xa);
        const double scale = (w > 1e-6) ? std::max(1.0, span / (m_config.baseIconSize * w)) : 1.0;
        const double displayCenterCanonical = 0.5 * (xa + xb);
        double primaryCenter = m_config.axis.canonicalToContainerPrimary(
            displayCenterCanonical, originP, restingLength
        );

        if (settledAtRest) {
            primaryCenter = snapToDevicePixel(primaryCenter, dpr);
        }

        const double visualSize = m_config.baseIconSize * scale * w;
        const double delegateScale = (scale * w) * invDelegateRatio;

        DockSlotLayout out{};
        out.id = slot.id;
        out.ghost = slot.ghost;
        out.weight = w;
        out.restingStart = a;
        out.restingEnd = b;
        out.displayStart = xa;
        out.displayEnd = xb;
        out.primaryCenter = primaryCenter;
        out.scale = scale;
        out.delegateScale = delegateScale;
        out.bounceOffset = slot.bounce.currentOffset;
        out.delegateTopLeft = m_config.axis.delegateTopLeft(
            primaryCenter,
            delegateSize,
            containerWidth,
            containerHeight,
            m_config.edgePadding,
            out.bounceOffset
        );

        // Static resting rect in container space (for KWin minimize geometry).
        const double restStartContainer = m_config.axis.canonicalToContainerPrimary(a, originP, restingLength);
        const double restEndContainer = m_config.axis.canonicalToContainerPrimary(b, originP, restingLength);
        out.restingRect = m_config.axis.makeEdgeRect(
            restStartContainer,
            restEndContainer,
            m_config.edgePadding,
            m_config.baseIconSize,
            containerWidth,
            containerHeight
        );

        // Live visual rect in container space (extended to screen edge for Fitts's Law hit-testing).
        const double dispStartContainer = m_config.axis.canonicalToContainerPrimary(xa, originP, restingLength);
        const double dispEndContainer = m_config.axis.canonicalToContainerPrimary(xb, originP, restingLength);
        out.displayRect = m_config.axis.makeEdgeRect(
            dispStartContainer,
            dispEndContainer,
            0.0,
            m_config.edgePadding + visualSize + out.bounceOffset,
            containerWidth,
            containerHeight
        );

        minDisplayP = std::min(minDisplayP, std::min(dispStartContainer, dispEndContainer));
        maxDisplayP = std::max(maxDisplayP, std::max(dispStartContainer, dispEndContainer));
        maxIconCrossExtent = std::max(maxIconCrossExtent, visualSize + out.bounceOffset);

        frame.items.push_back(out);

        // Advance resting cursor; last slot does not add trailing spacing.
        cursor = b + ((i + 1 < m_slots.size()) ? m_config.iconSpacing * w : 0.0);
    }

    // Pill background rect: rests at resting icon height + cross padding, expands along primary axis with icons.
    const double pillThickness = m_config.baseIconSize + 2.0 * m_config.pillPaddingCross;
    if (!frame.items.empty() && std::isfinite(minDisplayP) && std::isfinite(maxDisplayP)) {
        const double pillStart = minDisplayP - m_config.pillPaddingPrimary;
        const double pillEnd = maxDisplayP + m_config.pillPaddingPrimary;
        frame.pillRect = m_config.axis.makeEdgeRect(
            pillStart,
            pillEnd,
            std::max(0.0, m_config.edgePadding - m_config.pillPaddingCross),
            pillThickness,
            containerWidth,
            containerHeight
        );

        // Hover hull: extends from screen edge (0) to top of pill or magnified icons,
        // never triggering in empty transparent headroom above resting icons.
        const double hullCross = std::max(
            m_config.edgePadding + pillThickness,
            m_config.edgePadding + maxIconCrossExtent
        );
        frame.hoverHullRect = m_config.axis.makeEdgeRect(
            pillStart,
            pillEnd,
            0.0,
            hullCross,
            containerWidth,
            containerHeight
        );
    }

    return frame;
}

} // namespace fluctus::core
