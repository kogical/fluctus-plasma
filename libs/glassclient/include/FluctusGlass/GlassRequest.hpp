// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
//
// FluctusGlass client helper: asks the Fluctus Glass KWin effect to render Liquid Glass behind a
// region of one of this process's windows. Talks to the effect over D-Bus only (no GPL code).
//
//     auto *glass = new fluctus::glass::GlassRequest(this);
//     glass->setWindow(window);
//     glass->setRegion(QRectF(16, 16, 400, 64), 24);   // also manages the KWin blur region
//
// When the effect is not running, the window simply keeps a plain KWin blur region, so the
// same code degrades gracefully to frosted blur.
#pragma once

#include <QColor>
#include <QObject>
#include <QPointer>
#include <QRectF>
#include <QRegion>
#include <QString>
#include <QTimer>
#include <QVariantMap>
#include <QWindow>

#include <memory>

class QDBusServiceWatcher;
class QDBusPendingCallWatcher;

namespace fluctus::glass {

/// Material parameters. Defaults are the "liquid" recipe: a clear, thick refracting slab.
struct GlassParameters {
    /// "glass" (default), "plain" (frosted blur, opts out of global mode) or "none".
    QString mode{QStringLiteral("glass")};
    /// Master strength 0..2 scaling refraction, magnification, dispersion, bulge and highlights.
    qreal intensity{1.0};
    qreal radius{18.0};
    QColor tint{QColor::fromRgbF(1.0F, 1.0F, 1.0F, 0.015F)};
    qreal bezel{64.0};
    qreal refraction{60.0};
    qreal magnify{0.34};
    qreal edgeBend{0.15};
    qreal dispersion{0.74};
    qreal frost{0.24};
    qreal saturation{1.24};
    qreal brightness{1.02};
    qreal edgeShade{0.0};
    qreal grain{0.0};
    qreal highlight{0.84};
    qreal highlightWidth{1.85};
    qreal sheen{0.0};
    qreal lightAngle{-135.0};
    QColor innerBorder{QColor::fromRgbF(1.0F, 1.0F, 1.0F, 0.0F)};
    QColor outerBorder{QColor::fromRgbF(0.0F, 0.0F, 0.0F, 0.0F)};
    qreal borderWidth{1.0};
    QColor shadowColor{QColor::fromRgbF(0.0F, 0.0F, 0.0F, 0.09F)};
    qreal shadowBlur{36.0};
    qreal shadowOffsetX{0.0};
    qreal shadowOffsetY{8.0};
    qreal pointerLight{1.0};
    qreal pointerBulge{6.0};
    qreal pointerRadius{56.0};
    /// Pixels the blur region is grown by to obtain the glass rect (BlurRegionController insets 1 px).
    qreal inset{0.0};

    [[nodiscard]] QVariantMap toVariantMap() const;
    [[nodiscard]] static GlassParameters fromVariantMap(const QVariantMap &map, const GlassParameters &base);
    [[nodiscard]] static GlassParameters fromVariantMap(const QVariantMap &map);
    /// Fallback matching KWin's default Desktop Effects material when the effect is not running.
    [[nodiscard]] static GlassParameters globalFallback();
    /// Built-in looks: "light", "medium", "full", "liquid" (the defaults), "crystal", "plain",
    /// and "none". Tweak any field afterwards.
    [[nodiscard]] static GlassParameters preset(QStringView name);
    [[nodiscard]] bool operator==(const GlassParameters &) const = default;
};

class GlassRequest : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ isAvailable NOTIFY availableChanged)
    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)

public:
    explicit GlassRequest(QObject *parent = nullptr);
    ~GlassRequest() override;

    GlassRequest(const GlassRequest &) = delete;
    GlassRequest &operator=(const GlassRequest &) = delete;

    void setWindow(QWindow *window);
    [[nodiscard]] QWindow *window() const;

    void setEnabled(bool enabled);
    [[nodiscard]] bool isEnabled() const;

    void setParameters(const GlassParameters &parameters);
    [[nodiscard]] const GlassParameters &parameters() const;

    /// Extra protocol parameters merged over parameters() (see GlassProtocol.hpp).
    void setExtraParameters(const QVariantMap &extra);

    /// Window match hints merged over the automatic ones (size, app id, title).
    void setMatchHints(const QVariantMap &hints);

    /// Rounded glass region in window coordinates. When manageBlurRegion() is true (default)
    /// the helper also sets the matching KWin blur region, which doubles as the glass shape and
    /// as the frosted fallback when the effect is unavailable.
    void setRegion(const QRectF &rectInWindow, qreal radius);
    void setManageBlurRegion(bool manage);
    [[nodiscard]] bool managesBlurRegion() const;

    /// Only set the (rounded) blur region and never ask for glass: plain KWin blur, or Liquid Glass
    /// when the user runs Fluctus Glass in global mode.
    void setBlurOnly(bool blurOnly);
    [[nodiscard]] bool isBlurOnly() const;

    /// When true, omit per-surface optical parameters and let KWin apply its live global Desktop
    /// Effects material (still sending region/radius/shapes and extraParameters).
    void setFollowGlobal(bool follow);
    [[nodiscard]] bool followsGlobal() const;

    /// The effect's D-Bus service is present.
    [[nodiscard]] bool isAvailable() const;
    /// The effect has bound this request to our window and is rendering it.
    [[nodiscard]] bool isActive() const;
    /// KWin's EffectWindow UUID once attached.
    [[nodiscard]] QString windowUuid() const;

    /// Synchronous, bounded check for the effect's service (for startup decisions).
    [[nodiscard]] static bool serviceRegistered();

    /// Rounded QRegion helper (1 px inset, matching KWin blur conventions).
    [[nodiscard]] static QRegion roundedRegion(const QRectF &rect, qreal radius);

Q_SIGNALS:
    void availableChanged();
    void activeChanged();
    void enabledChanged();

private Q_SLOTS:
    void onAttached(const QString &handle, const QString &windowUuid);
    void onDetached(const QString &handle);

private:
    void scheduleSync();
    void sync();
    void registerNow();
    void unregisterNow();
    void applyBlurRegion();
    void setAvailable(bool available);
    void setActive(bool active, const QString &uuid = {});
    [[nodiscard]] QVariantMap matchMap() const;
    [[nodiscard]] QVariantMap paramMap() const;

    QPointer<QWindow> m_window;
    std::unique_ptr<QDBusServiceWatcher> m_watcher;
    QTimer m_syncTimer;
    GlassParameters m_params;
    QVariantMap m_extra;
    QVariantMap m_matchHints;
    QRectF m_region;
    qreal m_regionRadius{0.0};
    QString m_handle;
    QString m_windowUuid;
    QMetaObject::Connection m_sizeConnection;
    QMetaObject::Connection m_visibleConnection;
    bool m_enabled{true};
    bool m_available{false};
    bool m_active{false};
    bool m_registering{false};
    bool m_manageBlur{true};
    bool m_blurOnly{false};
    bool m_followGlobal{false};
    bool m_blurApplied{false};
};

} // namespace fluctus::glass
