#include "backend/FluctusApplet.hpp"
#include "backend/ShortcutManager.hpp"
#include "core/BouncePhysics.hpp"

namespace fluctus::backend {

FluctusApplet::FluctusApplet(
    QObject *parent,
    const KPluginMetaData &data,
    const QVariantList &args
)
    : Plasma::Applet(parent, data, args) {
    ShortcutManager::instance().registerApplet(this);
}

FluctusApplet::~FluctusApplet() {
    ShortcutManager::instance().unregisterApplet(this);
}

void FluctusApplet::setScreenGeometry(const QRect &geometry) {
    if (m_screenGeometry == geometry) {
        return;
    }
    m_screenGeometry = geometry;
    Q_EMIT screenGeometryChanged();
}

void FluctusApplet::setNeedsAttention(bool needsAttention) {
    setStatus(needsAttention ? Plasma::Types::NeedsAttentionStatus : Plasma::Types::ActiveStatus);
}

int FluctusApplet::recommendedPanelThickness(
    qreal baseIconSize,
    qreal maxScale,
    qreal bounceRatio,
    qreal edgePadding,
    qreal topMargin
) const {
    return core::requiredPanelThickness(
        baseIconSize,
        maxScale,
        bounceRatio,
        edgePadding,
        topMargin
    );
}

K_PLUGIN_CLASS_WITH_JSON(FluctusApplet, "../../package/metadata.json")

} // namespace fluctus::backend

#include "FluctusApplet.moc"
