// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
//
// Per-app Liquid Glass profile: preset selection ("global", "light", "medium", "full", "crystal",
// "plain", "none", or "custom"), live KWin global-defaults tracking over D-Bus, and single-string
// JSON serialisation for app/plasmoid config storage. QtCore/QtGui/QtDBus only.
#pragma once

#include "FluctusGlass/GlassRequest.hpp"

#include <QColor>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <memory>

class QDBusServiceWatcher;

namespace fluctus::glass {

class GlassProfile : public QObject {
    Q_OBJECT

    /// Active preset: "global" (follow KWin Desktop Effects), "light", "medium", "full",
    /// "crystal", "plain", "none", or "custom".
    Q_PROPERTY(QString preset READ preset WRITE applyPreset NOTIFY presetChanged)
    Q_PROPERTY(bool followGlobal READ followsGlobal NOTIFY presetChanged)
    Q_PROPERTY(bool available READ isAvailable NOTIFY availableChanged)
    /// Compact JSON representation suitable for storing in one KConfig/Plasmoid/QSettings key.
    Q_PROPERTY(QString json READ toJson WRITE loadJson NOTIFY jsonChanged)

    // User-tunable optical and pointer parameters (reflect live global defaults when preset == "global").
    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY parametersChanged)
    Q_PROPERTY(qreal intensity READ intensity WRITE setIntensity NOTIFY parametersChanged)
    Q_PROPERTY(qreal refraction READ refraction WRITE setRefraction NOTIFY parametersChanged)
    Q_PROPERTY(qreal bezel READ bezel WRITE setBezel NOTIFY parametersChanged)
    Q_PROPERTY(qreal magnify READ magnify WRITE setMagnify NOTIFY parametersChanged)
    Q_PROPERTY(qreal dispersion READ dispersion WRITE setDispersion NOTIFY parametersChanged)
    Q_PROPERTY(qreal frost READ frost WRITE setFrost NOTIFY parametersChanged)
    Q_PROPERTY(qreal saturation READ saturation WRITE setSaturation NOTIFY parametersChanged)
    Q_PROPERTY(qreal brightness READ brightness WRITE setBrightness NOTIFY parametersChanged)
    Q_PROPERTY(QColor tintColor READ tintColor WRITE setTintColor NOTIFY parametersChanged)
    Q_PROPERTY(qreal tintStrength READ tintStrength WRITE setTintStrength NOTIFY parametersChanged)
    Q_PROPERTY(qreal highlight READ highlight WRITE setHighlight NOTIFY parametersChanged)
    Q_PROPERTY(qreal edgeShade READ edgeShade WRITE setEdgeShade NOTIFY parametersChanged)
    Q_PROPERTY(qreal sheen READ sheen WRITE setSheen NOTIFY parametersChanged)
    Q_PROPERTY(bool pointerEffects READ pointerEffects WRITE setPointerEffects NOTIFY parametersChanged)
    Q_PROPERTY(qreal pointerBulge READ pointerBulge WRITE setPointerBulge NOTIFY parametersChanged)

public:
    explicit GlassProfile(QObject *parent = nullptr);
    ~GlassProfile() override;

    GlassProfile(const GlassProfile &) = delete;
    GlassProfile &operator=(const GlassProfile &) = delete;

    [[nodiscard]] QString preset() const { return m_preset; }
    [[nodiscard]] bool followsGlobal() const;
    [[nodiscard]] bool isAvailable() const { return m_available; }

    /// Effective parameters for the active preset (globalDefaults when following global).
    [[nodiscard]] const GlassParameters &parameters() const;
    void setParameters(const GlassParameters &parameters);

    [[nodiscard]] const GlassParameters &globalDefaults() const { return m_globalDefaults; }
    /// Test hook: inject global defaults without a running KWin session.
    void setGlobalDefaultsForTesting(const GlassParameters &defaults);

    [[nodiscard]] QVariantMap toVariantMap() const;
    void loadVariantMap(const QVariantMap &map);

    [[nodiscard]] QString toJson() const;
    Q_INVOKABLE bool loadJson(const QString &json);

    Q_INVOKABLE void applyPreset(const QString &name);
    Q_INVOKABLE void resetToGlobal();
    [[nodiscard]] Q_INVOKABLE static QStringList presetNames();

    [[nodiscard]] QString mode() const { return parameters().mode; }
    void setMode(const QString &value);
    [[nodiscard]] qreal intensity() const { return parameters().intensity; }
    void setIntensity(qreal value);
    [[nodiscard]] qreal refraction() const { return parameters().refraction; }
    void setRefraction(qreal value);
    [[nodiscard]] qreal bezel() const { return parameters().bezel; }
    void setBezel(qreal value);
    [[nodiscard]] qreal magnify() const { return parameters().magnify; }
    void setMagnify(qreal value);
    [[nodiscard]] qreal dispersion() const { return parameters().dispersion; }
    void setDispersion(qreal value);
    [[nodiscard]] qreal frost() const { return parameters().frost; }
    void setFrost(qreal value);
    [[nodiscard]] qreal saturation() const { return parameters().saturation; }
    void setSaturation(qreal value);
    [[nodiscard]] qreal brightness() const { return parameters().brightness; }
    void setBrightness(qreal value);
    [[nodiscard]] QColor tintColor() const;
    void setTintColor(const QColor &color);
    [[nodiscard]] qreal tintStrength() const { return parameters().tint.alphaF(); }
    void setTintStrength(qreal value);
    [[nodiscard]] qreal highlight() const { return parameters().highlight; }
    void setHighlight(qreal value);
    [[nodiscard]] qreal edgeShade() const { return parameters().edgeShade; }
    void setEdgeShade(qreal value);
    [[nodiscard]] qreal sheen() const { return parameters().sheen; }
    void setSheen(qreal value);
    [[nodiscard]] bool pointerEffects() const;
    void setPointerEffects(bool enabled);
    [[nodiscard]] qreal pointerBulge() const { return parameters().pointerBulge; }
    void setPointerBulge(qreal value);

Q_SIGNALS:
    void presetChanged();
    void parametersChanged();
    void jsonChanged();
    void globalDefaultsChanged();
    void availableChanged();

private Q_SLOTS:
    void onDefaultsChanged(const QVariantMap &defaults);

private:
    void fetchDefaults();
    void setAvailable(bool available);
    void applyGlobalDefaults(const GlassParameters &defaults);
    void beginCustomEdit();
    void setRealField(qreal GlassParameters::*field, qreal value, qreal lo, qreal hi);

    std::unique_ptr<QDBusServiceWatcher> m_watcher;
    QString m_preset{QStringLiteral("global")};
    GlassParameters m_params{GlassParameters::globalFallback()};
    GlassParameters m_globalDefaults{GlassParameters::globalFallback()};
    qreal m_savedPointerBulge{6.0};
    bool m_available{false};
};

} // namespace fluctus::glass
