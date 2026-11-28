#include "backend/ShortcutManager.hpp"
#include "backend/FluctusApplet.hpp"

#include <KGlobalAccel>
#include <KLocalizedString>

#include <QAction>
#include <QCursor>
#include <QGuiApplication>
#include <QScreen>

#include <algorithm>

namespace fluctus::backend {

ShortcutManager &ShortcutManager::instance() {
    static ShortcutManager mgr;
    return mgr;
}

ShortcutManager::ShortcutManager(QObject *parent)
    : QObject(parent) {}

void ShortcutManager::registerApplet(FluctusApplet *applet) {
    if (!applet) {
        return;
    }
    m_applets.erase(
        std::remove_if(m_applets.begin(), m_applets.end(), [](const QPointer<FluctusApplet> &p) {
            return p.isNull();
        }),
        m_applets.end()
    );
    if (!m_applets.contains(applet)) {
        m_applets.push_back(applet);
    }
    ensureActionsInitialized();
}

void ShortcutManager::unregisterApplet(FluctusApplet *applet) {
    m_applets.erase(
        std::remove_if(m_applets.begin(), m_applets.end(), [applet](const QPointer<FluctusApplet> &p) {
            return p.isNull() || p.data() == applet;
        }),
        m_applets.end()
    );
}

void ShortcutManager::ensureActionsInitialized() {
    if (m_initialized) {
        return;
    }
    m_initialized = true;

    // Reuse Plasma 6.8's stock taskmanager action names under plasmashell so user Meta+1..9
    // bindings carry over without duplicating System Settings entries.
    constexpr int kShortcutCount = 9;
    m_actions.reserve(kShortcutCount);
    for (int i = 1; i <= kShortcutCount; ++i) {
        auto *action = new QAction(this);
        action->setObjectName(QStringLiteral("activate task manager entry %1").arg(i));
        action->setText(i18n("Activate Task Manager Entry %1", i));
        action->setProperty("componentName", QStringLiteral("plasmashell"));
        action->setProperty("componentDisplayName", i18n("Plasma"));

        const QKeySequence defaultSeq(Qt::META | static_cast<Qt::Key>(Qt::Key_0 + i));
        KGlobalAccel::setGlobalShortcut(action, defaultSeq);

        const int zeroBasedIndex = i - 1;
        connect(action, &QAction::triggered, this, [this, zeroBasedIndex]() {
            activateTaskEntry(zeroBasedIndex);
        });
        m_actions.push_back(action);
    }
}

void ShortcutManager::activateTaskEntry(int zeroBasedIndex) {
    if (auto *target = selectTargetApplet()) {
        Q_EMIT target->activateTaskRequested(zeroBasedIndex);
    }
}

FluctusApplet *ShortcutManager::selectTargetApplet() {
    m_applets.erase(
        std::remove_if(m_applets.begin(), m_applets.end(), [](const QPointer<FluctusApplet> &p) {
            return p.isNull();
        }),
        m_applets.end()
    );
    if (m_applets.isEmpty()) {
        return nullptr;
    }

    // Prefer the applet on the screen containing the cursor, falling back to primary / first.
    const QPoint cursorPos = QCursor::pos();
    for (const auto &applet : m_applets) {
        if (applet && applet->screenGeometry().contains(cursorPos)) {
            return applet.data();
        }
    }

    if (const QScreen *primary = QGuiApplication::primaryScreen()) {
        const QRect primaryGeom = primary->geometry();
        for (const auto &applet : m_applets) {
            if (applet && applet->screenGeometry() == primaryGeom) {
                return applet.data();
            }
        }
    }

    return m_applets.first().data();
}

} // namespace fluctus::backend
