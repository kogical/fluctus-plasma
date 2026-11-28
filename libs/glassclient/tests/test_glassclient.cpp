// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
//
// Headless tests for Fluctus::GlassClient: GlassParameters presets/serialisation and
// GlassProfile preset switching, JSON persistence, and live global-default tracking.

#include "FluctusGlass/GlassProfile.hpp"
#include "FluctusGlass/GlassProtocol.hpp"
#include "FluctusGlass/GlassRequest.hpp"

#include <QSignalSpy>
#include <QTest>

using namespace fluctus::glass;

class TestGlassClient : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void presetsOrderStrengthAndRoundTrip() {
        const GlassParameters light = GlassParameters::preset(u"light");
        const GlassParameters medium = GlassParameters::preset(u"medium");
        const GlassParameters full = GlassParameters::preset(u"full");
        QVERIFY(light.intensity < medium.intensity);
        QVERIFY(medium.intensity < full.intensity);
        QVERIFY(light.refraction < medium.refraction);
        QVERIFY(medium.refraction < full.refraction);

        const GlassParameters roundTripped = GlassParameters::fromVariantMap(medium.toVariantMap());
        QCOMPARE(roundTripped.intensity, medium.intensity);
        QCOMPARE(roundTripped.refraction, medium.refraction);
        QCOMPARE(roundTripped.bezel, medium.bezel);
        QCOMPARE(roundTripped.dispersion, medium.dispersion);
        QCOMPARE(roundTripped.frost, medium.frost);

        QCOMPARE(GlassParameters::preset(u"plain").mode, QStringLiteral("plain"));
        QCOMPARE(GlassParameters::preset(u"none").mode, QStringLiteral("none"));
    }

    void profileStartsGlobalAndTracksLiveDefaults() {
        GlassProfile profile;
        QCOMPARE(profile.preset(), QStringLiteral("global"));
        QVERIFY(profile.followsGlobal());
        QCOMPARE(profile.toJson(), QStringLiteral("{\"preset\":\"global\"}"));

        QSignalSpy paramsSpy(&profile, &GlassProfile::parametersChanged);
        GlassParameters customGlobal = GlassParameters::globalFallback();
        customGlobal.refraction = 42.0;
        customGlobal.frost = 0.35;
        profile.setGlobalDefaultsForTesting(customGlobal);

        QCOMPARE(paramsSpy.count(), 1);
        QCOMPARE(profile.refraction(), 42.0);
        QCOMPARE(profile.frost(), 0.35);
    }

    void profilePresetsCustomEditAndReset() {
        GlassProfile profile;
        GlassParameters global = GlassParameters::globalFallback();
        global.refraction = 30.0;
        profile.setGlobalDefaultsForTesting(global);

        profile.applyPreset(QStringLiteral("medium"));
        QCOMPARE(profile.preset(), QStringLiteral("medium"));
        QVERIFY(!profile.followsGlobal());
        QCOMPARE(profile.toJson(), QStringLiteral("{\"preset\":\"medium\"}"));

        // Editing a slider flips to "custom" starting from the active preset values.
        const qreal mediumBezel = profile.bezel();
        profile.setRefraction(77.0);
        QCOMPARE(profile.preset(), QStringLiteral("custom"));
        QCOMPARE(profile.refraction(), 77.0);
        QCOMPARE(profile.bezel(), mediumBezel);

        // JSON round-trip preserves custom values.
        const QString saved = profile.toJson();
        GlassProfile restored;
        QVERIFY(restored.loadJson(saved));
        QCOMPARE(restored.preset(), QStringLiteral("custom"));
        QCOMPARE(restored.refraction(), 77.0);
        QCOMPARE(restored.bezel(), mediumBezel);

        // Reset to global restores live global defaults.
        profile.resetToGlobal();
        QCOMPARE(profile.preset(), QStringLiteral("global"));
        QVERIFY(profile.followsGlobal());
        QCOMPARE(profile.refraction(), 30.0);
    }

    void profilePointerToggleRemembersBulge() {
        GlassProfile profile;
        profile.setPointerBulge(14.0);
        QVERIFY(profile.pointerEffects());
        profile.setPointerEffects(false);
        QVERIFY(!profile.pointerEffects());
        QCOMPARE(profile.pointerBulge(), 0.0);
        profile.setPointerEffects(true);
        QVERIFY(profile.pointerEffects());
        QCOMPARE(profile.pointerBulge(), 14.0);
    }

    void profileRejectsMalformedJson() {
        GlassProfile profile;
        profile.applyPreset(QStringLiteral("full"));
        QVERIFY(!profile.loadJson(QStringLiteral("{not json")));
        QCOMPARE(profile.preset(), QStringLiteral("full"));
        // Empty string resets to global.
        QVERIFY(profile.loadJson(QString()));
        QCOMPARE(profile.preset(), QStringLiteral("global"));
    }
};

QTEST_GUILESS_MAIN(TestGlassClient)

#include "test_glassclient.moc"
