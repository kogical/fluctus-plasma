#pragma once

#include <Plasma/Applet>
#include <QRect>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

namespace fluctus::backend {

/// Compiled Plasma::Applet entry point for Fluctus Plasma.
/// Exposes clean C++ helpers to the thin QML shell (headroom math, attention status,
/// screen geometry tracking, SmartLauncher badges, Jump List actions, and global shortcuts).
class FluctusApplet final : public Plasma::Applet {
    Q_OBJECT

    Q_PROPERTY(QRect screenGeometry READ screenGeometry WRITE setScreenGeometry NOTIFY screenGeometryChanged)

public:
    FluctusApplet(QObject *parent, const KPluginMetaData &data, const QVariantList &args);
    ~FluctusApplet() override;

    [[nodiscard]] QRect screenGeometry() const noexcept { return m_screenGeometry; }
    void setScreenGeometry(const QRect &geometry);

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
    void activateTaskRequested(int index);
    void smartLauncherUpdated(const QString &normalizedAppId);

private:
    QRect m_screenGeometry;
};

} // namespace fluctus::backend
