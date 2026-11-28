// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "backend/BatteryDeviceModel.hpp"
#include "backend/ScintillaBridge.hpp"

#include <Plasma/Applet>
#include <QString>
#include <QTimer>
#include <QVariantMap>

#include <memory>

class QDBusServiceWatcher;

namespace fluctus::battery::backend {

/// Compiled Plasma::Applet for Fluctus Battery (`com.kogical.fluctus.battery`).
/// Bridges the async `scintilla` peripheral battery backend, system D-Bus power/Bluetooth
/// signals, and the pure C++23 adaptive layout engine to the declarative Liquid Glass QML shell.
class BatteryApplet final : public Plasma::Applet {
    Q_OBJECT

    Q_PROPERTY(QObject *devicesModel READ devicesModel CONSTANT)
    /// Fluctus Glass, the KWin effect behind real Liquid Glass:
    /// 0 = not installed, 1 = installed but turned off, 2 = running.
    Q_PROPERTY(int glassEffectState READ glassEffectState NOTIFY glassEffectStateChanged)
    /// Shell command that installs the udev rule letting the widget read vendor HID batteries.
    Q_PROPERTY(QString udevRuleCommand READ udevRuleCommand CONSTANT)

public:
    BatteryApplet(QObject *parent, const KPluginMetaData &data, const QVariantList &args);
    ~BatteryApplet() override;

    void init() override;

    [[nodiscard]] QObject *devicesModel() const noexcept { return m_model.get(); }
    [[nodiscard]] int glassEffectState() const noexcept { return m_glassEffectState; }
    [[nodiscard]] static QString udevRuleCommand();

    /// Re-checks whether Fluctus Glass is installed and running.
    Q_INVOKABLE void refreshGlassEffectState();
    /// Loads (or unloads) the Fluctus Glass KWin effect.
    Q_INVOKABLE void setFluctusGlassEnabled(bool enabled);
    Q_INVOKABLE void openDesktopEffectsSettings() const;
    Q_INVOKABLE void copyToClipboard(const QString &text) const;

    /// Triggers an immediate asynchronous device & battery discovery pass.
    Q_INVOKABLE void refresh();

    /// Updates filtering, sorting, and warning thresholds from Plasmoid.configuration.
    Q_INVOKABLE void setFilterConfig(
        bool showStubs,
        bool hideLinePower,
        int sortOrder,
        const QString &preferredDeviceId,
        int lowThreshold,
        int criticalThreshold
    );

    /// Sets the background polling interval in seconds (clamped to 10..600s).
    Q_INVOKABLE void setPollIntervalSeconds(int seconds);

    /// Computes adaptive breakpoint & typography/spacing metrics for the given container size.
    [[nodiscard]] Q_INVOKABLE QVariantMap layoutMetrics(
        qreal width,
        qreal height,
        bool inPanel,
        bool verticalPanel
    ) const;

Q_SIGNALS:
    void glassEffectStateChanged();

private Q_SLOTS:
    void scheduleDebouncedRefresh();

private:
    void connectSystemSignals();

    std::unique_ptr<BatteryDeviceModel> m_model;
    std::unique_ptr<ScintillaBridge> m_bridge;
    QTimer m_debounceTimer;
    QTimer m_pollTimer;
    std::unique_ptr<QDBusServiceWatcher> m_glassWatcher;
    int m_glassEffectState{0};
};

} // namespace fluctus::battery::backend
