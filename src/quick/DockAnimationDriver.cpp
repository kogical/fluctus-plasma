#include "quick/DockAnimationDriver.hpp"

#include <algorithm>
#include <utility>

namespace fluctus::quick {

DockAnimationDriver::DockAnimationDriver(QObject *parent)
    : QAbstractAnimation(parent) {}

void DockAnimationDriver::setTickCallback(TickCallback callback) {
    m_tickCallback = std::move(callback);
}

void DockAnimationDriver::ensureRunning() {
    if (state() != QAbstractAnimation::Running) {
        m_hasLastTime = false;
        m_lastTimeMs = 0;
        start();
    }
}

void DockAnimationDriver::stopImmediately() {
    if (state() != QAbstractAnimation::Stopped) {
        stop();
    }
    m_hasLastTime = false;
    m_lastTimeMs = 0;
}

void DockAnimationDriver::updateState(
    QAbstractAnimation::State newState,
    QAbstractAnimation::State oldState
) {
    Q_UNUSED(oldState);
    if (newState != QAbstractAnimation::Running) {
        m_hasLastTime = false;
        m_lastTimeMs = 0;
    }
}

void DockAnimationDriver::updateCurrentTime(int currentTime) {
    if (!m_tickCallback) {
        stopImmediately();
        return;
    }

    double dt = 1.0 / 60.0;
    if (m_hasLastTime) {
        const int deltaMs = std::max(0, currentTime - m_lastTimeMs);
        dt = static_cast<double>(deltaMs) / 1000.0;
    }
    m_lastTimeMs = currentTime;
    m_hasLastTime = true;

    if (dt <= 0.0) {
        return;
    }

    const bool keepRunning = m_tickCallback(dt);
    if (!keepRunning) {
        stopImmediately();
    }
}

} // namespace fluctus::quick
