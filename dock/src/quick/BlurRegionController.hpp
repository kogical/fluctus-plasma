#pragma once

#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QQuickItem>
#include <QRectF>
#include <QRegion>
#include <QTimer>
#include <QWindow>

namespace fluctus::quick {

/// Background contrast parameters forwarded to KWindowEffects::enableBackgroundContrast.
struct BlurContrast {
    bool enabled{false};
    qreal contrast{1.0};
    qreal intensity{1.0};
    qreal saturation{1.0};

    [[nodiscard]] bool operator==(const BlurContrast &) const = default;
};

/// Manages KWindowEffects blur-behind + background contrast for the dock pill on Plasma 6.8 Wayland.
/// - Clears blur on the previous QWindow when the item changes windows.
/// - Insets the rounded QRegion by ~1 px so integer logical QRegion never bleeds past
///   antialiased pill corners under fractional scaling.
/// - Coalesces every change into at most one compositor update per event-loop pass, sending
///   blur and contrast together so they never drift apart during hover animation.
/// - Re-applies after Plasma's PanelView rewrites the window's blur (on resize, reposition,
///   theme change, floating animation and panelMask change — PanelView::updateMask always
///   overwrites blur/contrast on the shared panel window).
class BlurRegionController final : public QObject {
    Q_OBJECT

public:
    explicit BlurRegionController(QObject *parent = nullptr);
    ~BlurRegionController() override;

    void setWindow(QWindow *window);
    void setEnabled(bool enabled);
    void setCornerRadius(qreal radius);
    void setWindowPillRect(const QRectF &rectInWindow);
    /// Optional explicit blur shape in window coordinates (e.g. a Plasma FrameSvg mask).
    /// When non-empty it replaces the rounded-rect region.
    void setCustomRegion(const QRegion &regionInWindow);
    void setContrast(const BlurContrast &contrast);

    /// Applies immediately (used by the coalescing timer and tests).
    void applyNow();

    [[nodiscard]] static QRegion buildRoundedRegion(const QRectF &rect, qreal cornerRadius);
    [[nodiscard]] QRegion currentRegion() const;

public Q_SLOTS:
    /// Schedules a coalesced re-application (next event-loop pass).
    void reapply();

private:
    /// Region Plasma's own panel background blurs (empty when the panel has no background),
    /// so enabling the pill blur never strips blur from a visible panel background.
    [[nodiscard]] QRegion panelBackgroundRegion() const;

    void clearBlurOnWindow(QWindow *window);
    void watchPanelRoot();

    QPointer<QWindow> m_window;
    QPointer<QQuickItem> m_panelRoot;
    QList<QMetaObject::Connection> m_panelConnections;
    QTimer m_applyTimer;
    bool m_enabled{true};
    bool m_appliedBlur{false};
    bool m_appliedContrast{false};
    qreal m_cornerRadius{16.0};
    QRectF m_windowPillRect;
    QRegion m_customRegion;
    BlurContrast m_contrast;
};

} // namespace fluctus::quick
