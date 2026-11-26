#pragma once

#include <QObject>
#include <QPointer>
#include <QRectF>
#include <QRegion>
#include <QWindow>

namespace fluctus::quick {

/// Manages KWindowEffects::enableBlurBehind for the dock pill on Plasma 6.8 Wayland.
/// - Clears blur on the previous QWindow when the item changes windows.
/// - Insets the rounded QRegion by ~1 px so integer logical QRegion never bleeds past
///   antialiased pill corners under fractional scaling.
/// - Re-applies blur when the window resizes or pill geometry changes (countering PanelView resets).
class BlurRegionController final : public QObject {
    Q_OBJECT

public:
    explicit BlurRegionController(QObject *parent = nullptr);
    ~BlurRegionController() override;

    void setWindow(QWindow *window);
    void setEnabled(bool enabled);
    void setCornerRadius(qreal radius);
    void setWindowPillRect(const QRectF &rectInWindow);
    void reapply();

    [[nodiscard]] static QRegion buildRoundedRegion(const QRectF &rect, qreal cornerRadius);

private:
    void clearBlurOnWindow(QWindow *window);

    QPointer<QWindow> m_window;
    bool m_enabled{true};
    qreal m_cornerRadius{16.0};
    QRectF m_windowPillRect;
};

} // namespace fluctus::quick
