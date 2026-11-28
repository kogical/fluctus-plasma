#pragma once

#include <QObject>
#include <QPointer>
#include <QVector>

class QAction;

namespace fluctus::backend {

class FluctusApplet;

/// Process-wide global shortcut router for Meta+1..9 task activation.
/// Holds non-owning QPointer<FluctusApplet> entries so destroying an applet instance
/// never leaves a dangling pointer (fixing WaveTask's static once_flag bug).
class ShortcutManager final : public QObject {
    Q_OBJECT

public:
    static ShortcutManager &instance();

    void registerApplet(FluctusApplet *applet);
    void unregisterApplet(FluctusApplet *applet);

private:
    explicit ShortcutManager(QObject *parent = nullptr);
    void ensureActionsInitialized();
    void activateTaskEntry(int zeroBasedIndex);
    [[nodiscard]] FluctusApplet *selectTargetApplet();

    QVector<QPointer<FluctusApplet>> m_applets;
    QVector<QAction *> m_actions;
    bool m_initialized{false};
};

} // namespace fluctus::backend
