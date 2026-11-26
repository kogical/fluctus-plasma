#include "quick/BlurRegionController.hpp"

#include <KWindowEffects>
#include <QPainterPath>
#include <QPolygon>

#include <algorithm>
#include <cmath>

namespace fluctus::quick {

BlurRegionController::BlurRegionController(QObject *parent)
    : QObject(parent) {}

BlurRegionController::~BlurRegionController() {
    clearBlurOnWindow(m_window);
}

void BlurRegionController::setWindow(QWindow *window) {
    if (m_window == window) {
        return;
    }

    if (m_window) {
        disconnect(m_window, nullptr, this, nullptr);
        clearBlurOnWindow(m_window);
    }

    m_window = window;

    if (m_window) {
        connect(m_window, &QWindow::widthChanged, this, &BlurRegionController::reapply);
        connect(m_window, &QWindow::heightChanged, this, &BlurRegionController::reapply);
        connect(m_window, &QWindow::screenChanged, this, &BlurRegionController::reapply);
        connect(m_window, &QWindow::visibleChanged, this, &BlurRegionController::reapply);
        reapply();
    }
}

void BlurRegionController::setEnabled(bool enabled) {
    if (m_enabled == enabled) {
        return;
    }
    m_enabled = enabled;
    reapply();
}

void BlurRegionController::setCornerRadius(qreal radius) {
    const qreal clamped = std::max<qreal>(0.0, radius);
    if (qFuzzyCompare(m_cornerRadius, clamped)) {
        return;
    }
    m_cornerRadius = clamped;
    reapply();
}

void BlurRegionController::setWindowPillRect(const QRectF &rectInWindow) {
    if (m_windowPillRect == rectInWindow) {
        return;
    }
    m_windowPillRect = rectInWindow;
    reapply();
}

QRegion BlurRegionController::buildRoundedRegion(const QRectF &rect, qreal cornerRadius) {
    // Inset by 1 logical px so integer QRegion never bleeds past antialiased corners
    // under fractional scaling (125%, 150%, etc.).
    const QRectF inset = rect.adjusted(1.0, 1.0, -1.0, -1.0);
    if (inset.width() <= 2.0 || inset.height() <= 2.0) {
        return {};
    }

    const qreal maxR = std::min(inset.width(), inset.height()) * 0.5;
    const qreal r = std::clamp<qreal>(cornerRadius - 1.0, 0.0, maxR);
    if (r <= 0.5) {
        return QRegion(inset.toAlignedRect());
    }

    QPainterPath path;
    path.addRoundedRect(inset, r, r);
    const QPolygon polygon = path.toFillPolygon().toPolygon();
    return QRegion(polygon);
}

void BlurRegionController::clearBlurOnWindow(QWindow *window) {
    if (window) {
        KWindowEffects::enableBlurBehind(window, false);
    }
}

void BlurRegionController::reapply() {
    if (!m_window || !m_window->isVisible()) {
        return;
    }

    if (!m_enabled || m_windowPillRect.isEmpty()) {
        KWindowEffects::enableBlurBehind(m_window, false);
        return;
    }

    const QRegion region = buildRoundedRegion(m_windowPillRect, m_cornerRadius);
    if (region.isEmpty()) {
        KWindowEffects::enableBlurBehind(m_window, false);
        return;
    }

    KWindowEffects::enableBlurBehind(m_window, true, region);
}

} // namespace fluctus::quick
