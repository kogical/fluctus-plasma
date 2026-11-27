#include "core/AxisGeometry.hpp"
#include "core/BouncePhysics.hpp"
#include "core/DampedSpring.hpp"
#include "core/DockPhysicsEngine.hpp"
#include "core/MagnificationCurves.hpp"

#include <QtTest/QtTest>

#include <array>
#include <cmath>

using namespace fluctus::core;

namespace {

// Composite Simpson's rule to verify closed-form antiderivatives W(u).
double simpsonIntegral(MagnificationCurve curve, double u, int steps = 1000) {
    if (u <= 0.0) {
        return 0.0;
    }
    const double h = u / static_cast<double>(steps);
    double sum = curveWeight(curve, 0.0) + curveWeight(curve, u);
    for (int i = 1; i < steps; ++i) {
        const double x = static_cast<double>(i) * h;
        sum += (i % 2 == 0 ? 2.0 : 4.0) * curveWeight(curve, x);
    }
    return sum * h / 3.0;
}

} // namespace

class DockPhysicsTest : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testMagnificationCurvesBoundaryAndClosedFormIntegral() {
        const std::array<MagnificationCurve, 3> curves{
            MagnificationCurve::RaisedCosine,
            MagnificationCurve::QuinticSmootherstep,
            MagnificationCurve::WindowedGaussian,
        };

        for (const auto curve : curves) {
            QCOMPARE(curveWeight(curve, 0.0), 1.0);
            QCOMPARE(curveWeight(curve, -0.5), 1.0);
            QCOMPARE(curveWeight(curve, 1.0), 0.0);
            QCOMPARE(curveWeight(curve, 1.5), 0.0);
            QCOMPARE(curveIntegral(curve, 0.0), 0.0);

            for (double u : {0.1, 0.25, 0.5, 0.75, 0.9, 1.0}) {
                const double analytic = curveIntegral(curve, u);
                const double numeric = simpsonIntegral(curve, u);
                QVERIFY2(
                    std::abs(analytic - numeric) < 1e-7,
                    qPrintable(QStringLiteral("Integral mismatch at u=%1: analytic=%2 numeric=%3")
                                   .arg(u).arg(analytic).arg(numeric))
                );
            }

            // Integral stays constant for u > 1 (finite support).
            QCOMPARE(curveIntegral(curve, 1.5), curveIntegral(curve, 1.0));
        }

        // Raised cosine exact W(1) == 0.5
        QVERIFY(std::abs(curveTotalIntegral(MagnificationCurve::RaisedCosine) - 0.5) < 1e-12);
        QVERIFY(std::abs(maxSideSlack(MagnificationCurve::RaisedCosine, 1.8, 100.0) - 40.0) < 1e-12);
    }

    void testDampedSpringAllThreeRegimes() {
        // Test critical (exact & near-critical boundary), underdamped, and overdamped.
        for (double zeta : {0.5, 0.99995, 1.0, 1.00005, 1.5}) {
            DampedSpring spring{};
            spring.snap(0.0);
            spring.target = 1.0;

            for (int step = 0; step < 300; ++step) {
                spring.step(1.0 / 60.0, 22.0, zeta);
                QVERIFY(std::isfinite(spring.value));
                QVERIFY(std::isfinite(spring.velocity));
            }
            QVERIFY2(
                std::abs(spring.value - 1.0) < 1e-3,
                qPrintable(QStringLiteral("Spring failed to converge for zeta=%1: value=%2")
                               .arg(zeta).arg(spring.value))
            );
        }

        // Critically damped spring must not overshoot below 0 or above 1.
        DampedSpring crit{};
        crit.snap(0.0);
        crit.target = 1.0;
        for (int i = 0; i < 120; ++i) {
            crit.step(1.0 / 60.0, 22.0, 1.0);
            QVERIFY(crit.value >= -1e-9);
            QVERIFY(crit.value <= 1.0 + 1e-9);
        }

        // Large dt hitch must be clamped safely.
        DampedSpring hitch{};
        hitch.snap(0.0);
        hitch.target = 1.0;
        hitch.step(5.0, 22.0, 1.0);
        QVERIFY(std::isfinite(hitch.value));
        QVERIFY(hitch.value > 0.0 && hitch.value < 1.0);
    }

    void testBounceLatchedHeightAndCleanDescent() {
        BounceState bounce{};
        bounce.setActive(true);

        // Start hop with desiredHeight = 40, headroom = 30 -> latchedHeight should clamp to 30.
        bounce.step(0.016, 40.0, 30.0);
        QVERIFY(bounce.inHop);
        QCOMPARE(bounce.latchedHeight, 30.0);

        // Change desiredHeight mid-hop (simulating hover zoom change): latchedHeight must NOT change!
        bounce.step(0.016, 80.0, 80.0);
        QCOMPARE(bounce.latchedHeight, 30.0);
        QVERIFY(bounce.currentOffset > 0.0 && bounce.currentOffset <= 30.0);

        // Clear active mid-hop: hop must finish its descent cleanly before going idle.
        bounce.setActive(false);
        for (int i = 0; i < 60; ++i) {
            bounce.step(0.016, 40.0, 30.0);
        }
        QVERIFY(bounce.isIdle());
        QCOMPARE(bounce.currentOffset, 0.0);

        // Required panel thickness formula check:
        // s0=48, M=1.8 -> ceil(86.4)=87; bounce=0.55*86.4=47.52 -> ceil=48; +6+6 = 147.
        QCOMPARE(requiredPanelThickness(48.0, 1.8, 0.55, 6.0, 6.0), 147);
    }

    void testZeroCursorDriftAndMonotonicity() {
        DockPhysicsEngine engine{};
        DockConfig cfg{};
        cfg.baseIconSize = 48.0;
        cfg.iconSpacing = 6.0;
        cfg.maxScale = 1.8;
        cfg.influenceRadiusIcons = 2.5;
        engine.setConfig(cfg);

        for (std::size_t i = 0; i < 8; ++i) {
            engine.insertSlot(i, false);
        }

        const double W = 900.0;
        const double H = 160.0;

        // Zero cursor drift: X(p) == p identically for any p and any alpha.
        for (double p : {0.0, 24.0, 110.0, 213.5, 426.0}) {
            QCOMPARE(engine.mapCanonicalCoordinate(p, p, 1.0), p);
        }

        // Engage pointer over the dock pill and step to full engagement.
        engine.updatePointer(Vec2{450.0, H - 24.0}, W, H);
        for (int i = 0; i < 120; ++i) {
            engine.step(1.0 / 60.0, W, H);
        }

        const DockFrame frame = engine.computeFrame(W, H);
        QVERIFY(frame.hoverAlpha > 0.99);
        QCOMPARE(frame.items.size(), std::size_t{8});

        // Strict monotonicity & zero overlap between adjacent icons.
        for (std::size_t i = 0; i + 1 < frame.items.size(); ++i) {
            QVERIFY(frame.items[i].displayEnd > frame.items[i].displayStart);
            QVERIFY(frame.items[i + 1].displayStart > frame.items[i].displayEnd);
            QVERIFY(frame.items[i].scale >= 1.0);
        }

        // Pill width must never exceed pre-allocated implicitPrimary.
        QVERIFY(frame.pillRect.width <= frame.implicitPrimary + 1e-6);
    }

    void testHoverHeadroomIgnoredAndCleanPointerExit() {
        DockPhysicsEngine engine{};
        for (std::size_t i = 0; i < 5; ++i) {
            engine.insertSlot(i, false);
        }

        const double W = 800.0;
        const double H = 160.0;

        // Point high up in empty transparent headroom (y = 10 on Bottom edge) must NOT engage hover.
        engine.updatePointer(Vec2{400.0, 10.0}, W, H);
        QVERIFY(!engine.isPointerEngaged());

        // Point inside the visible dock pill (y = H - 20) MUST engage hover.
        engine.updatePointer(Vec2{400.0, H - 20.0}, W, H);
        QVERIFY(engine.isPointerEngaged());
        for (int i = 0; i < 60; ++i) {
            engine.step(1.0 / 60.0, W, H);
        }
        QVERIFY(engine.computeFrame(W, H).hoverAlpha > 0.9);

        // Clearing pointer collapses smoothly back to alpha == 0 and settles.
        engine.clearPointer();
        for (int i = 0; i < 180; ++i) {
            engine.step(1.0 / 60.0, W, H);
        }
        QVERIFY(engine.isSettled());
        QCOMPARE(engine.computeFrame(W, H).hoverAlpha, 0.0);
    }

    void testGhostSlotRemovalAndRestingInsertionIndex() {
        DockPhysicsEngine engine{};
        for (std::size_t i = 0; i < 5; ++i) {
            engine.insertSlot(i, false);
        }

        const double W = 800.0;
        const double H = 160.0;

        // Resting insertion index must be identical whether zoomed or at rest.
        const std::size_t idxAtRest = engine.restingInsertionIndex(Vec2{400.0, H - 20.0}, W, H);
        engine.updatePointer(Vec2{400.0, H - 20.0}, W, H);
        for (int i = 0; i < 60; ++i) {
            engine.step(1.0 / 60.0, W, H);
        }
        const std::size_t idxZoomed = engine.restingInsertionIndex(Vec2{400.0, H - 20.0}, W, H);
        QCOMPARE(idxZoomed, idxAtRest);

        // Animated removal creates a ghost slot that shrinks to 0 and is pruned.
        engine.removeSlot(2, true);
        QCOMPARE(engine.activeSlotCount(), std::size_t{4});
        QCOMPARE(engine.totalSlotCount(), std::size_t{5});

        for (int i = 0; i < 180; ++i) {
            engine.step(1.0 / 60.0, W, H);
        }
        QCOMPARE(engine.activeSlotCount(), std::size_t{4});
        QCOMPARE(engine.totalSlotCount(), std::size_t{4});
    }

    void testAllFourEdgesAndReversePrimary() {
        for (DockEdge edge : {DockEdge::Bottom, DockEdge::Top, DockEdge::Left, DockEdge::Right}) {
            DockPhysicsEngine engine{};
            DockConfig cfg{};
            cfg.axis.edge = edge;
            cfg.axis.reversePrimary = false;
            engine.setConfig(cfg);
            for (std::size_t i = 0; i < 4; ++i) {
                engine.insertSlot(i, false);
            }

            const double W = cfg.axis.isHorizontal() ? 800.0 : 160.0;
            const double H = cfg.axis.isHorizontal() ? 160.0 : 800.0;
            const DockFrame normal = engine.computeFrame(W, H);
            QCOMPARE(normal.items.size(), std::size_t{4});
            QVERIFY(normal.items[0].primaryCenter < normal.items[3].primaryCenter);

            cfg.axis.reversePrimary = true;
            engine.setConfig(cfg);
            const DockFrame reversed = engine.computeFrame(W, H);
            QVERIFY(reversed.items[0].primaryCenter > reversed.items[3].primaryCenter);
        }
    }

    void testPanelAutoScalingAndContainerClamping() {
        DockPhysicsEngine engine{};
        DockConfig cfg{};
        cfg.baseIconSize = 48.0;
        cfg.iconSpacing = 6.0;
        cfg.maxScale = 1.8;
        cfg.edgePadding = 4.0;
        cfg.topMargin = 4.0;
        cfg.autoScaleToPanel = true;
        cfg.clampToContainer = true;
        engine.setConfig(cfg);

        for (std::size_t i = 0; i < 6; ++i) {
            engine.insertSlot(i, false);
        }

        // Compact 48 px Plasma panel: effective icon size & zoom must fit inside 40 px available cross-axis.
        const double smallH = 48.0;
        const double smallBase = engine.effectiveBaseIconSize(800.0, smallH);
        const double smallMax = engine.effectiveMaxScaleForContainer(800.0, smallH);
        QVERIFY(smallBase >= 20.0 && smallBase <= 40.0);
        QVERIFY(smallBase * smallMax <= (smallH - cfg.edgePadding - cfg.topMargin) + 1e-6);

        // Tall 120 px Plasma panel: effective base icon size scales up proportionally with panel height.
        const double tallH = 120.0;
        const double tallBase = engine.effectiveBaseIconSize(800.0, tallH);
        QVERIFY(tallBase > smallBase);

        // Explicit proportional panel ratios: 50% resting icon, 90% max zoom (1.8x), 20% spacing.
        cfg.panelIconRatio = 0.50;
        cfg.maxZoomPanelRatio = 0.90;
        cfg.iconSpacingRatio = 0.20;
        engine.setConfig(cfg);
        const double halfBase = engine.effectiveBaseIconSize(800.0, 108.0);
        const double targetZoom = engine.effectiveMaxScaleForContainer(800.0, 108.0);
        const double propSpacing = engine.effectiveIconSpacing(800.0, 108.0);
        QVERIFY(std::abs(halfBase - 50.0) < 1e-6);
        QVERIFY(std::abs(targetZoom - 1.8) < 1e-6);
        QVERIFY(std::abs(propSpacing - 10.0) < 1e-6);

        // Narrow primary container: icons compress smoothly so pill never overflows container width.
        const double narrowW = 180.0;
        const DockFrame narrowFrame = engine.computeFrame(narrowW, 80.0);
        QVERIFY(narrowFrame.pillRect.width <= narrowW + 1e-6);
    }

    void testSmoothSpringReorderAndInsertionIndicator() {
        DockPhysicsEngine engine{};
        DockConfig cfg{};
        cfg.baseIconSize = 48.0;
        cfg.iconSpacing = 6.0;
        cfg.maxScale = 1.6;
        engine.setConfig(cfg);

        for (std::size_t i = 0; i < 4; ++i) {
            engine.insertSlot(i, false);
        }

        const double W = 800.0;
        const double H = 160.0;
        const DockFrame before = engine.computeFrame(W, H);
        const double oldSlot0Center = before.items[0].primaryCenter;
        const double oldSlot2Center = before.items[2].primaryCenter;

        // Move slot 0 -> index 2: engine should become unsettled and glide smoothly from old position!
        engine.moveSlot(0, 2);
        QVERIFY(!engine.isSettled());

        const DockFrame rightAfterMove = engine.computeFrame(W, H);
        // Right after move (t=0), the moved slot starts at its old center (zero teleport!).
        QVERIFY(std::abs(rightAfterMove.items[2].primaryCenter - oldSlot0Center) < 1.0);

        // After stepping 2 seconds, all reorder springs settle cleanly at exact resting positions.
        for (int i = 0; i < 120; ++i) {
            engine.step(1.0 / 60.0, W, H);
        }
        QVERIFY(engine.isSettled());
        const DockFrame settled = engine.computeFrame(W, H);
        QVERIFY(std::abs(settled.items[2].primaryCenter - oldSlot2Center) < 1.0);

        // Insertion indicator rect must have valid non-zero geometry inside the container.
        const Rect ind = engine.insertionIndicatorRect(2, W, H);
        QVERIFY(ind.width > 0.0 && ind.height > 0.0);
        QVERIFY(ind.x >= 0.0 && ind.x + ind.width <= W);

        // Drag target index hysteresis: hovering near a slot boundary must not oscillate.
        const double boundaryX = 0.5 * (settled.items[0].restingRect.x + settled.items[0].restingRect.width
                                        + settled.items[1].restingRect.x);
        QCOMPARE(engine.restingDragTargetIndex(Vec2{boundaryX + 2.0, H - 10.0}, W, H, 0), std::size_t{0});
        QCOMPARE(engine.restingDragTargetIndex(Vec2{boundaryX + 20.0, H - 10.0}, W, H, 0), std::size_t{1});
        QCOMPARE(engine.restingDragTargetIndex(Vec2{boundaryX - 2.0, H - 10.0}, W, H, 1), std::size_t{1});
    }

    void testCompactRestingPixelAlignment() {
        // Odd compact icon size at 1x and fractional 1.5x DPR must align the visual leading edge
        // (and cross-axis edge base) to exact physical pixels when settled at rest.
        for (double dpr : {1.0, 1.5, 2.0}) {
            DockPhysicsEngine engine{};
            DockConfig cfg{};
            cfg.baseIconSize = 23.0;
            cfg.iconSpacing = 4.0;
            cfg.maxScale = 1.5;
            cfg.edgePadding = 5.0;
            cfg.topMargin = 5.0;
            cfg.devicePixelRatio = dpr;
            engine.setConfig(cfg);

            for (std::size_t i = 0; i < 6; ++i) {
                engine.insertSlot(i, false);
            }

            const double W = 400.0;
            const double H = 45.0;
            engine.step(1.0 / 60.0, W, H);
            QVERIFY(engine.isSettled());

            const DockFrame frame = engine.computeFrame(W, H);
            QCOMPARE(frame.items.size(), std::size_t{6});
            for (const auto &slot : frame.items) {
                const double visualSize = cfg.baseIconSize * slot.scale * slot.weight;
                const double visualLeadPx = (slot.primaryCenter - visualSize * 0.5) * dpr;
                QVERIFY2(
                    std::abs(visualLeadPx - std::round(visualLeadPx)) < 1e-6,
                    qPrintable(QStringLiteral("Unaligned visual lead at dpr=%1: %2 px")
                                   .arg(dpr).arg(visualLeadPx))
                );
                const double delegateSize = engine.effectiveDelegateLogicalSize(W, H);
                const double visualBottomPx = (slot.delegateTopLeft.y + delegateSize) * dpr;
                QVERIFY2(
                    std::abs(visualBottomPx - std::round(visualBottomPx)) < 1e-6,
                    qPrintable(QStringLiteral("Unaligned visual bottom at dpr=%1: %2 px")
                                   .arg(dpr).arg(visualBottomPx))
                );
            }
        }
    }
};

QTEST_MAIN(DockPhysicsTest)
#include "test_dock_physics.moc"
