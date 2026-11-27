#include "backend/FluctusApplet.hpp"
#include "backend/ShortcutManager.hpp"
#include "backend/SmartLauncherBackend.hpp"
#include "backend/TaskBackend.hpp"
#include "core/BouncePhysics.hpp"

#include <KModifierKeyInfo>

namespace fluctus::backend {

FluctusApplet::FluctusApplet(
    QObject *parent,
    const KPluginMetaData &data,
    const QVariantList &args
)
    : Plasma::Applet(parent, data, args)
    , m_modifierKeyInfo(std::make_unique<KModifierKeyInfo>()) {
    ShortcutManager::instance().registerApplet(this);
    connect(
        &SmartLauncherBackend::instance(),
        &SmartLauncherBackend::entryChanged,
        this,
        &FluctusApplet::smartLauncherUpdated
    );
    m_metaKeyPressed = m_modifierKeyInfo->isKeyPressed(Qt::Key_Meta)
        || m_modifierKeyInfo->isKeyPressed(Qt::Key_Super_L)
        || m_modifierKeyInfo->isKeyPressed(Qt::Key_Super_R);
    connect(
        m_modifierKeyInfo.get(),
        &KModifierKeyInfo::keyPressed,
        this,
        [this](Qt::Key key, bool /*pressed*/) {
            if (key != Qt::Key_Meta && key != Qt::Key_Super_L && key != Qt::Key_Super_R) {
                return;
            }
            const bool nowPressed = m_modifierKeyInfo->isKeyPressed(Qt::Key_Meta)
                || m_modifierKeyInfo->isKeyPressed(Qt::Key_Super_L)
                || m_modifierKeyInfo->isKeyPressed(Qt::Key_Super_R);
            if (m_metaKeyPressed != nowPressed) {
                m_metaKeyPressed = nowPressed;
                Q_EMIT metaKeyPressedChanged();
            }
        }
    );
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

QVariantMap FluctusApplet::smartLauncherState(const QString &appId, const QUrl &launcherUrl) const {
    return SmartLauncherBackend::instance().stateForApp(appId, launcherUrl.toString());
}

QVariantList FluctusApplet::jumpListActions(const QUrl &launcherUrl) const {
    return TaskBackend::jumpListActions(launcherUrl);
}

bool FluctusApplet::triggerJumpListAction(const QUrl &launcherUrl, const QString &actionName) const {
    return TaskBackend::triggerJumpListAction(launcherUrl, actionName);
}

K_PLUGIN_CLASS_WITH_JSON(FluctusApplet, "../../package/metadata.json")

} // namespace fluctus::backend

#include "FluctusApplet.moc"
