// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Headless tests for Fluctus Glass's KWin-independent logic: blur-region islands, corner radius
// estimation, parameter parsing and D-Bus registration ownership.

#include "glassservice.hpp"
#include "material.hpp"
#include "shapes.hpp"

#include "FluctusGlass/GlassProtocol.hpp"

#include <QPainterPath>
#include <QPolygon>
#include <QRegion>
#include <QSignalSpy>
#include <QTest>

using namespace fluctus::glass;
namespace p = fluctus::glass::protocol::param;

namespace {

std::vector<QRectF> roundedRects(const QRectF &rect, qreal radius) {
    QPainterPath path;
    path.addRoundedRect(rect, radius, radius);
    const QRegion region(path.toFillPolygon().toPolygon());
    std::vector<QRectF> out;
    for (const QRect &r : region) {
        out.emplace_back(r);
    }
    return out;
}

} // namespace

class TestGlassLogic : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void singleRectIsOneSquareIsland() {
        const std::vector<QRectF> rects{QRectF(10, 10, 200, 50)};
        const auto islands = findIslands(rects);
        QCOMPARE(islands.size(), std::size_t{1});
        QCOMPARE(islands[0].rect, QRectF(10, 10, 200, 50));
        QCOMPARE(islands[0].radius, 0.0);
    }

    void roundedRegionIsOneIslandWithRadius() {
        const auto rects = roundedRects(QRectF(20, 30, 400, 64), 18);
        QVERIFY(rects.size() > 4);
        const auto islands = findIslands(rects);
        QCOMPARE(islands.size(), std::size_t{1});
        QVERIFY(qAbs(islands[0].rect.width() - 400) <= 1.5);
        QVERIFY(qAbs(islands[0].rect.height() - 64) <= 1.5);
        QVERIFY2(qAbs(islands[0].radius - 18) <= 2.5, qPrintable(QString::number(islands[0].radius)));
    }

    void separatePiecesAreSeparateIslands() {
        auto rects = roundedRects(QRectF(0, 0, 100, 40), 10);
        const auto other = roundedRects(QRectF(200, 0, 100, 40), 10);
        rects.insert(rects.end(), other.begin(), other.end());
        const auto islands = findIslands(rects);
        QCOMPARE(islands.size(), std::size_t{2});
    }

    void pillNeverReachingFullWidthUsesHalfHeight() {
        const auto rects = roundedRects(QRectF(0, 0, 200, 40), 20);
        const auto islands = findIslands(rects);
        QCOMPARE(islands.size(), std::size_t{1});
        QVERIFY(islands[0].radius >= 18.0);
        QVERIFY(islands[0].radius <= 20.5);
    }

    void matchPicksBestOverlap() {
        const std::vector<Island> islands{
            {.rect = QRectF(0, 0, 100, 40), .radius = 0},
            {.rect = QRectF(200, 0, 100, 40), .radius = 0},
        };
        QCOMPARE(matchIsland(islands, QRectF(198, 1, 104, 38)), 1);
        QCOMPARE(matchIsland(islands, QRectF(500, 500, 10, 10)), -1);
        QCOMPARE(intersectionOverUnion(QRectF(), QRectF(0, 0, 1, 1)), 0.0);
    }

    void materialParsesAndClamps() {
        const QVariantMap map{
            {QString(p::kRadius), 24.0},
            {QString(p::kRefraction), 9999.0},
            {QString(p::kTint), QStringLiteral("#80ff0000")},
            {QString(p::kFrost), QStringLiteral("not a number")},
            {QString(p::kShapes), QVariantList{1.0, 2.0, 300.0, 40.0, 12.0, 0.0, 0.0, -5.0, 10.0, 1.0}},
        };
        const Material m = Material::fromVariantMap(map);
        QCOMPARE(m.radius, 24.0);
        QCOMPARE(m.refraction, 128.0);
        QCOMPARE(m.tint.red(), 255);
        QCOMPARE(m.tint.alpha(), 128);
        QCOMPARE(m.frost, Material{}.frost);
        QCOMPARE(m.shapes.size(), std::size_t{1}); // negative width rejected
        QCOMPARE(m.shapes[0].rect, QRectF(1, 2, 300, 40));
        QVERIFY(m.sampleMargin() > m.refraction);
    }

    void materialModeAndIntensity() {
        const QVariantMap map{
            {QString(p::kMode), QStringLiteral("plain")},
            {QString(p::kIntensity), 0.5},
            {QString(p::kRefraction), 20.0},
        };
        const Material m = Material::fromVariantMap(map);
        QCOMPARE(m.mode, Material::Mode::Plain);
        const Material s = m.scaled();
        QCOMPARE(s.refraction, 10.0);
        QCOMPARE(s.intensity, 1.0);
        QCOMPARE(Material::fromVariantMap({{QString(p::kMode), QStringLiteral("bogus")}}).mode, Material::Mode::Glass);
        QCOMPARE(Material::fromVariantMap({{QString(p::kIntensity), 9.0}}).intensity, 2.0);
    }

    void presetsAreBaseOverriddenByKeys() {
        const Material crystal = Material::fromVariantMap({{QString(p::kPreset), QStringLiteral("crystal")}});
        QCOMPARE(crystal.edgeBend, 0.25);
        QVERIFY(crystal.frost < Material{}.frost);
        const Material tuned = Material::fromVariantMap({{QString(p::kPreset), QStringLiteral("crystal")}, {QString(p::kFrost), 0.5}});
        QCOMPARE(tuned.frost, 0.5);
        QCOMPARE(Material::preset(u"liquid"), Material{});

        const Material light = Material::preset(u"light");
        const Material medium = Material::preset(u"medium");
        const Material full = Material::preset(u"full");
        QVERIFY(light.intensity < medium.intensity);
        QVERIFY(medium.intensity < full.intensity);

        const Material roundTripped = Material::fromVariantMap(medium.toVariantMap());
        QCOMPARE(roundTripped.intensity, medium.intensity);
        QCOMPARE(roundTripped.refraction, medium.refraction);
        QCOMPARE(roundTripped.bezel, medium.bezel);
    }

    void serviceExposesDefaultsAndUpdatesFollowGlobalRegistrations() {
        GlassService service;
        QSignalSpy defaultsSpy(&service, &GlassService::DefaultsChanged);
        QStringList changes;
        service.setChangeListener([&changes](const QString &handle) { changes.append(handle); });

        Material initial;
        initial.refraction = 24.0;
        initial.frost = 0.15;
        service.setGlobalMaterial(initial);
        QCOMPARE(defaultsSpy.count(), 1);
        QCOMPARE(Material::fromVariantMap(service.GetDefaults()).refraction, 24.0);

        const QString h = service.registerFor(
            QStringLiteral(":1.20"), {},
            {{QString(p::kFollowGlobal), true}, {QString(p::kRadius), 16.0}});
        QVERIFY(!h.isEmpty());
        QVERIFY(service.find(h)->followGlobal);
        QCOMPARE(service.find(h)->material.refraction, 24.0);
        QCOMPARE(service.find(h)->material.radius, 16.0);

        // Updating KWin's global material re-resolves followGlobal registrations live.
        Material updated = initial;
        updated.refraction = 48.0;
        updated.frost = 0.40;
        changes.clear();
        service.setGlobalMaterial(updated);
        QCOMPARE(defaultsSpy.count(), 2);
        QVERIFY(changes.contains(h));
        QCOMPARE(service.find(h)->material.refraction, 48.0);
        QCOMPARE(service.find(h)->material.frost, 0.40);
        QCOMPARE(service.find(h)->material.radius, 16.0);
    }

    void materialIgnoresNonFiniteShapes() {
        const QVariantMap map{{QString(p::kShapes), QVariantList{qQNaN(), 0.0, 10.0, 10.0, 1.0}}};
        QVERIFY(Material::fromVariantMap(map).shapes.empty());
    }

    void serviceTracksOwnershipAndBinding() {
        GlassService service;
        QStringList changes;
        service.setChangeListener([&changes](const QString &handle) { changes.append(handle); });
        QSignalSpy attached(&service, &GlassService::Attached);
        QSignalSpy detached(&service, &GlassService::Detached);

        const QString a = service.registerFor(QStringLiteral(":1.10"), {}, {{QString(p::kRadius), 12.0}});
        const QString b = service.registerFor(QStringLiteral(":1.11"), {}, {});
        QVERIFY(!a.isEmpty());
        QVERIFY(a != b);
        QCOMPARE(service.find(a)->material.radius, 12.0);
        QVERIFY(!service.find(a)->pid.has_value());

        service.setPidForTesting(a, 4242);
        QCOMPARE(*service.find(a)->pid, 4242);

        const QUuid window = QUuid::createUuid();
        service.bind(a, window);
        QCOMPARE(attached.count(), 1);
        service.bind(a, window); // no duplicate signal
        QCOMPARE(attached.count(), 1);
        service.unbindWindow(window);
        QCOMPARE(detached.count(), 1);
        QVERIFY(service.find(a)->window.isNull());
        QVERIFY(changes.contains(a));
    }

    void serviceLimitsRegistrationsPerClient() {
        GlassService service;
        for (int i = 0; i < fluctus::glass::protocol::kMaxRegistrationsPerClient; ++i) {
            QVERIFY(!service.registerFor(QStringLiteral(":1.5"), {}, {}).isEmpty());
        }
        QVERIFY(service.registerFor(QStringLiteral(":1.5"), {}, {}).isEmpty());
        QVERIFY(!service.registerFor(QStringLiteral(":1.6"), {}, {}).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestGlassLogic)

#include "test_glass_logic.moc"
