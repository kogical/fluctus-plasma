#pragma once

#include <QAbstractAnimation>
#include <functional>

namespace fluctus::quick {

/// Strictly GUI-thread animation driver backed by QAbstractAnimation (duration == -1).
/// Never runs on the Qt Quick render thread. Computes dt from Qt's animation clock
/// and stops automatically when the tick callback reports that physics have settled.
class DockAnimationDriver final : public QAbstractAnimation {
    Q_OBJECT

public:
    using TickCallback = std::function<bool(double dtSeconds)>;

    explicit DockAnimationDriver(QObject *parent = nullptr);

    void setTickCallback(TickCallback callback);
    void ensureRunning();
    void stopImmediately();

    [[nodiscard]] int duration() const override { return -1; }

protected:
    void updateCurrentTime(int currentTime) override;
    void updateState(QAbstractAnimation::State newState, QAbstractAnimation::State oldState) override;

private:
    TickCallback m_tickCallback;
    int m_lastTimeMs{0};
    bool m_hasLastTime{false};
};

} // namespace fluctus::quick
