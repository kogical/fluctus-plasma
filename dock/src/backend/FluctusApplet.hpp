#pragma once

#include <Plasma/Applet>
#include <Plasma/Containment>
#include <QPointer>
#include <KConfigWatcher>
#include <QRect>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include <memory>

class KModifierKeyInfo;

class QDBusServiceWatcher;

namespace fluctus::backend {

/// Compiled Plasma::Applet entry point for Fluctus Plasma.
/// Exposes clean C++ helpers to the thin QML shell (headroom math, attention status,
/// screen geometry tracking, SmartLauncher badges, Jump List actions, and global shortcuts).
class FluctusApplet final : public Plasma::Applet {
    Q_OBJECT

    Q_PROPERTY(QRect screenGeometry READ screenGeometry WRITE setScreenGeometry NOTIFY screenGeometryChanged)
    Q_PROPERTY(bool metaKeyPressed READ metaKeyPressed NOTIFY metaKeyPressedChanged)
    /// When true, switches the hosting panel to Plasma::Types::NoBackground (no panel frame,
    /// blur, contrast or shadow) and restores the original hints when turned off or removed.
    Q_PROPERTY(bool hidePanelBackground READ hidePanelBackground WRITE setHidePanelBackground NOTIFY panelBackgroundStateChanged)
    /// True when another widget in the same panel (Panel Colorizer) owns the panel background;
    /// Fluctus then leaves it untouched.
    Q_PROPERTY(bool panelBackgroundManagedExternally READ panelBackgroundManagedExternally NOTIFY panelBackgroundStateChanged)
    /// True when Fluctus currently hides the panel background.
    Q_PROPERTY(bool panelBackgroundHidden READ panelBackgroundHidden NOTIFY panelBackgroundStateChanged)
    /// Plasma's "Panel Height" (PanelView::thickness, logical px); 0 when not in a panel.
    Q_PROPERTY(int panelThickness READ panelThickness NOTIFY panelGeometryChanged)
    /// Cross-axis size the dock actually receives inside the panel (logical px), reported by QML.
    Q_PROPERTY(qreal dockCrossExtent READ dockCrossExtent WRITE setDockCrossExtent NOTIFY panelGeometryChanged)
    /// KWin's system-wide blur strength (Effect-blur/BlurStrength in kwinrc, 1..15). Writing it
    /// changes blur for the whole desktop and reloads the blur effect immediately.
    Q_PROPERTY(int systemBlurStrength READ systemBlurStrength WRITE setSystemBlurStrength NOTIFY systemBlurStrengthChanged)
    /// State of the best compositor glass provider, which Liquid Glass needs for real refraction.
    /// Detection order: Fluctus Glass (our KWin effect), then the third-party kwin-effects-glass.
    /// 0 = neither installed, 1 = installed but not running, 2 = running.
    Q_PROPERTY(int glassEffectState READ glassEffectState NOTIFY glassEffectStateChanged)
    /// Which provider glassEffectState describes: "fluctusglass", "glass" (kwin-effects-glass) or "".
    Q_PROPERTY(QString glassProvider READ glassProvider NOTIFY glassEffectStateChanged)
    /// Linux distribution family from /etc/os-release ("arch", "gentoo" or "other"),
    /// used to show the right install instructions.
    Q_PROPERTY(QString distroFamily READ distroFamily CONSTANT)

public:
    FluctusApplet(QObject *parent, const KPluginMetaData &data, const QVariantList &args);
    ~FluctusApplet() override;

    [[nodiscard]] QRect screenGeometry() const noexcept { return m_screenGeometry; }
    void setScreenGeometry(const QRect &geometry);
    [[nodiscard]] bool metaKeyPressed() const noexcept { return m_metaKeyPressed; }

    void init() override;

    [[nodiscard]] bool hidePanelBackground() const noexcept { return m_hidePanelBackground; }
    void setHidePanelBackground(bool hide);
    [[nodiscard]] bool panelBackgroundManagedExternally() const noexcept { return m_panelBgExternal; }
    [[nodiscard]] bool panelBackgroundHidden() const noexcept { return m_panelBgApplied; }

    [[nodiscard]] int panelThickness() const;
    [[nodiscard]] qreal dockCrossExtent() const noexcept { return m_dockCrossExtent; }
    void setDockCrossExtent(qreal extent);
    /// Registers the PanelView hosting the dock (called from main.qml with Window.window).
    Q_INVOKABLE void setPanelWindow(QObject *window);
    /// Sets Plasma's panel height; PanelView persists it itself. Returns false outside a panel.
    Q_INVOKABLE bool setPanelThickness(int thickness);

    [[nodiscard]] int systemBlurStrength() const;
    /// Invokable so QML can call it: the attached Plasmoid object is statically typed as
    /// Plasma::Applet, so assigning subclass properties from QML is silently ignored.
    Q_INVOKABLE void setSystemBlurStrength(int strength);

    [[nodiscard]] int glassEffectState() const noexcept { return m_glassEffectState; }
    [[nodiscard]] QString glassProvider() const { return m_glassProvider; }
    /// Loads (or unloads) an installed KWin effect in the running session, e.g. "fluctusglass".
    Q_INVOKABLE void setKWinEffectEnabled(const QString &effectId, bool enabled) const;
    [[nodiscard]] QString distroFamily() const;
    /// Re-checks whether the Glass effect is installed and running (asynchronous).
    Q_INVOKABLE void refreshGlassEffectState();
    /// Opens System Settings on the Desktop Effects page.
    Q_INVOKABLE void openDesktopEffectsSettings() const;
    Q_INVOKABLE void copyToClipboard(const QString &text) const;

    Q_INVOKABLE void setNeedsAttention(bool needsAttention);
    Q_INVOKABLE int recommendedPanelThickness(
        qreal baseIconSize,
        qreal maxScale,
        qreal bounceRatio,
        qreal edgePadding,
        qreal topMargin
    ) const;

    Q_INVOKABLE QVariantMap smartLauncherState(const QString &appId, const QUrl &launcherUrl) const;
    Q_INVOKABLE QVariantList jumpListActions(const QUrl &launcherUrl) const;
    Q_INVOKABLE bool triggerJumpListAction(const QUrl &launcherUrl, const QString &actionName) const;

Q_SIGNALS:
    void screenGeometryChanged();
    void metaKeyPressedChanged();
    void activateTaskRequested(int index);
    void smartLauncherUpdated(const QString &normalizedAppId);
    void panelBackgroundStateChanged();
    void panelGeometryChanged();
    void systemBlurStrengthChanged();
    void glassEffectStateChanged();

private:
    void syncPanelBackground();
    void attachContainment();
    void restorePanelBackground();

    QRect m_screenGeometry;
    QPointer<Plasma::Containment> m_panelContainment;
    QPointer<QObject> m_panelWindow;
    QMetaObject::Connection m_thicknessConnection;
    qreal m_dockCrossExtent{0.0};
    void applyGlassState(int state, const QString &provider);

    int m_glassEffectState{0};
    QString m_glassProvider;
    std::unique_ptr<QDBusServiceWatcher> m_fluctusGlassWatcher;
    KConfigWatcher::Ptr m_kwinConfigWatcher;
    Plasma::Types::BackgroundHints m_savedPanelHints{Plasma::Types::StandardBackground};
    bool m_hidePanelBackground{false};
    bool m_panelBgApplied{false};
    bool m_panelBgExternal{false};
    std::unique_ptr<KModifierKeyInfo> m_modifierKeyInfo;
    bool m_metaKeyPressed{false};
};

} // namespace fluctus::backend
