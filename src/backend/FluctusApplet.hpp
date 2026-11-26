#pragma once

#include <Plasma/Applet>
#include <QRect>

namespace fluctus::backend {

/// Compiled Plasma::Applet entry point for Fluctus Plasma.
/// Exposes clean C++ helpers to the thin QML shell (headroom math, attention status,
/// screen geometry tracking, and global shortcut routing).
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

Q_SIGNALS:
    void screenGeometryChanged();
    void activateTaskRequested(int index);

private:
    QRect m_screenGeometry;
};

} // namespace fluctus::backend
