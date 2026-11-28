#include "backend/FluctusApplet.hpp"

#include "backend/ShortcutManager.hpp"
#include "backend/SmartLauncherBackend.hpp"
#include "backend/TaskBackend.hpp"
#include "core/BouncePhysics.hpp"

#include <KConfigGroup>
#include <KModifierKeyInfo>
#include <KSharedConfig>
#include <QClipboard>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QProcess>
#include <QTimer>

#include <algorithm>
#include <array>

namespace {
// Widgets known to take ownership of the panel background; Fluctus defers to them.
constexpr std::array kPanelBackgroundOwners{
    QLatin1StringView("luisbocanegra.panel.colorizer"),
};

const QString kGlassEffectId = QStringLiteral("glass");

bool glassEffectInstalled() {
    // kwin-effects-glass installs glass.so into <plugin dir>/kwin/effects/plugins/.
    const QStringList pluginDirs = QCoreApplication::libraryPaths();
    for (const QString &dir : pluginDirs) {
        const QDir effects(dir + QStringLiteral("/kwin/effects/plugins"));
        if (!effects.entryList({kGlassEffectId + QStringLiteral(".*")}, QDir::Files).isEmpty()) {
            return true;
        }
    }
    return false;
}

QDBusMessage isGlassLoadedMessage() {
    QDBusMessage message = QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.KWin"),
        QStringLiteral("/Effects"),
        QStringLiteral("org.kde.kwin.Effects"),
        QStringLiteral("isEffectLoaded")
    );
    message << kGlassEffectId;
    return message;
}

bool glassEffectLoaded(QDBus::CallMode mode) {
    constexpr int kTimeoutMs = 500;
    const QDBusReply<bool> reply = QDBusConnection::sessionBus().call(isGlassLoadedMessage(), mode, kTimeoutMs);
    return reply.isValid() && reply.value();
}
} // namespace

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
    restorePanelBackground();
    ShortcutManager::instance().unregisterApplet(this);
}

void FluctusApplet::init() {
    Plasma::Applet::init();
    attachContainment();

    // Enabling/disabling desktop effects rewrites kwinrc [Plugins]; re-check the Glass effect.
    m_kwinConfigWatcher = KConfigWatcher::create(
        KSharedConfig::openConfig(QStringLiteral("kwinrc"), KConfig::NoGlobals)
    );
    connect(m_kwinConfigWatcher.data(), &KConfigWatcher::configChanged, this,
        [this](const KConfigGroup &group) {
            if (group.name() == QLatin1String("Plugins")) {
                refreshGlassEffectState();
            }
        });
    // First check is synchronous (bounded) so a Liquid Glass dock never starts as Frosted.
    m_glassEffectState = glassEffectInstalled() ? (glassEffectLoaded(QDBus::Block) ? 2 : 1) : 0;
    // The containment can finish wiring its applets after init(); re-check once settled.
    QTimer::singleShot(0, this, &FluctusApplet::attachContainment);
}

void FluctusApplet::attachContainment() {
    Plasma::Containment *c = containment();
    if (c == m_panelContainment) {
        syncPanelBackground();
        return;
    }
    if (m_panelContainment) {
        restorePanelBackground();
        disconnect(m_panelContainment, nullptr, this, nullptr);
    }
    m_panelContainment = c;
    if (c) {
        connect(c, &Plasma::Containment::appletAdded, this, &FluctusApplet::syncPanelBackground);
        connect(c, &Plasma::Containment::appletRemoved, this, [this](Plasma::Applet *removed) {
            if (removed == this) {
                restorePanelBackground();
                return;
            }
            QTimer::singleShot(0, this, &FluctusApplet::syncPanelBackground);
        });
        connect(c, &Plasma::Applet::backgroundHintsChanged, this, [this] {
            // Someone else (e.g. the user via another widget) changed the hints while we hold
            // them hidden: treat their value as the new baseline to restore.
            if (m_panelBgApplied && m_panelContainment
                && m_panelContainment->backgroundHints() != Plasma::Types::NoBackground) {
                m_savedPanelHints = m_panelContainment->backgroundHints();
                m_panelBgApplied = false;
                syncPanelBackground();
            }
        });
    }
    syncPanelBackground();
}

int FluctusApplet::panelThickness() const {
    if (!m_panelWindow) {
        return 0;
    }
    const QVariant value = m_panelWindow->property("thickness");
    return value.isValid() ? value.toInt() : 0;
}

void FluctusApplet::setDockCrossExtent(qreal extent) {
    if (qFuzzyCompare(m_dockCrossExtent + 1.0, extent + 1.0)) {
        return;
    }
    m_dockCrossExtent = extent;
    Q_EMIT panelGeometryChanged();
}

void FluctusApplet::setPanelWindow(QObject *window) {
    if (m_panelWindow == window) {
        return;
    }
    disconnect(m_thicknessConnection);
    m_panelWindow = nullptr;
    // Only PanelView exposes a "thickness" property; ignore desktop/other hosts.
    if (window && window->metaObject()->indexOfProperty("thickness") >= 0) {
        m_panelWindow = window;
        const int signalIndex = window->metaObject()->indexOfSignal("thicknessChanged()");
        if (signalIndex >= 0) {
            m_thicknessConnection = connect(
                window,
                window->metaObject()->method(signalIndex),
                this,
                metaObject()->method(metaObject()->indexOfSignal("panelGeometryChanged()"))
            );
        }
    }
    Q_EMIT panelGeometryChanged();
}

bool FluctusApplet::setPanelThickness(int thickness) {
    if (!m_panelWindow || thickness <= 0) {
        return false;
    }
    return m_panelWindow->setProperty("thickness", thickness);
}

namespace {
constexpr int kDefaultBlurStrength = 10; // KWin's default
KConfigGroup kwinBlurGroup() {
    KSharedConfig::Ptr config = KSharedConfig::openConfig(QStringLiteral("kwinrc"), KConfig::NoGlobals);
    config->reparseConfiguration();
    return config->group(QStringLiteral("Effect-blur"));
}
} // namespace

int FluctusApplet::systemBlurStrength() const {
    return std::clamp(kwinBlurGroup().readEntry("BlurStrength", kDefaultBlurStrength), 1, 15);
}

void FluctusApplet::setSystemBlurStrength(int strength) {
    const int clamped = std::clamp(strength, 1, 15);
    if (clamped == systemBlurStrength()) {
        return;
    }
    KConfigGroup group = kwinBlurGroup();
    group.writeEntry("BlurStrength", clamped, KConfig::Notify);
    group.sync();
    QDBusMessage message = QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.KWin"),
        QStringLiteral("/Effects"),
        QStringLiteral("org.kde.kwin.Effects"),
        QStringLiteral("reconfigureEffect")
    );
    message << QStringLiteral("blur");
    QDBusConnection::sessionBus().asyncCall(message);
    Q_EMIT systemBlurStrengthChanged();
}



void FluctusApplet::refreshGlassEffectState() {
    if (!glassEffectInstalled()) {
        if (m_glassEffectState != 0) {
            m_glassEffectState = 0;
            Q_EMIT glassEffectStateChanged();
        }
        return;
    }
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(isGlassLoadedMessage()), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *call) {
        const QDBusPendingReply<bool> reply = *call;
        const int state = (reply.isValid() && reply.value()) ? 2 : 1;
        if (state != m_glassEffectState) {
            m_glassEffectState = state;
            Q_EMIT glassEffectStateChanged();
        }
        call->deleteLater();
    });
}

QString FluctusApplet::distroFamily() const {
    QFile osRelease(QStringLiteral("/etc/os-release"));
    if (!osRelease.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QStringLiteral("other");
    }
    QString ids;
    while (!osRelease.atEnd()) {
        const QString line = QString::fromUtf8(osRelease.readLine()).trimmed();
        if (line.startsWith(QLatin1String("ID=")) || line.startsWith(QLatin1String("ID_LIKE="))) {
            ids += QLatin1Char(' ') + line.section(QLatin1Char('='), 1).remove(QLatin1Char('"'));
        }
    }
    const QStringList tokens = ids.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (tokens.contains(QLatin1String("arch"))) {
        return QStringLiteral("arch");
    }
    if (tokens.contains(QLatin1String("gentoo"))) {
        return QStringLiteral("gentoo");
    }
    return QStringLiteral("other");
}

void FluctusApplet::openDesktopEffectsSettings() const {
    QProcess::startDetached(QStringLiteral("systemsettings"), {QStringLiteral("kcm_kwin_effects")});
}

void FluctusApplet::copyToClipboard(const QString &text) const {
    if (QClipboard *clipboard = QGuiApplication::clipboard()) {
        clipboard->setText(text);
    }
}

void FluctusApplet::setHidePanelBackground(bool hide) {
    if (m_hidePanelBackground == hide) {
        return;
    }
    m_hidePanelBackground = hide;
    syncPanelBackground();
    Q_EMIT panelBackgroundStateChanged();
}

void FluctusApplet::syncPanelBackground() {
    Plasma::Containment *c = m_panelContainment;
    bool external = false;
    if (c) {
        const auto applets = c->applets();
        for (const Plasma::Applet *applet : applets) {
            if (applet == this) {
                continue;
            }
            const QString name = applet->pluginName();
            for (const auto owner : kPanelBackgroundOwners) {
                if (name == owner) {
                    external = true;
                }
            }
        }
    }
    const bool isPanel = c
        && (c->containmentType() == Plasma::Containment::Panel
            || c->containmentType() == Plasma::Containment::CustomPanel);
    const bool want = m_hidePanelBackground && isPanel && !external;

    if (want && !m_panelBgApplied) {
        m_savedPanelHints = c->backgroundHints();
        m_panelBgApplied = true; // set before the change so our own hint signal is ignored
        c->setBackgroundHints(Plasma::Types::NoBackground);
    } else if (!want && m_panelBgApplied) {
        restorePanelBackground();
    }

    if (m_panelBgExternal != external) {
        m_panelBgExternal = external;
    }
    Q_EMIT panelBackgroundStateChanged();
}

void FluctusApplet::restorePanelBackground() {
    if (!m_panelBgApplied) {
        return;
    }
    m_panelBgApplied = false;
    if (m_panelContainment && m_panelContainment->backgroundHints() == Plasma::Types::NoBackground) {
        m_panelContainment->setBackgroundHints(m_savedPanelHints);
    }
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
