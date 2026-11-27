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

void BlurRegionController::setBlurStyle(int style) {
    const int clamped = std::clamp(style, 0, 4);
    if (m_blurStyle == clamped) {
        return;
    }
    m_blurStyle = clamped;
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
    if (!window) {
        return;
    }
    if (m_appliedBlur) {
        KWindowEffects::enableBlurBehind(window, false);
        m_appliedBlur = false;
    }
    if (m_appliedContrast) {
        KWindowEffects::enableBackgroundContrast(window, false);
        m_appliedContrast = false;
    }
}

void BlurRegionController::reapply() {
    if (!m_window || !m_window->isVisible()) {
        return;
    }

    if (!m_enabled || m_blurStyle == 4 || m_windowPillRect.isEmpty()) {
        clearBlurOnWindow(m_window);
        return;
    }

    const QRegion region = buildRoundedRegion(m_windowPillRect, m_cornerRadius);
    if (region.isEmpty()) {
        clearBlurOnWindow(m_window);
        return;
    }

    KWindowEffects::enableBlurBehind(m_window, true, region);
    m_appliedBlur = true;

    // Apply compositor background contrast presets per blur style:
    // 0 = Frosted Glass, 1 = Liquid Glass (vibrant/refractive), 2 = Acrylic, 3 = Clear Blur (blur only)
    switch (m_blurStyle) {
    case 0:
        KWindowEffects::enableBackgroundContrast(m_window, true, 1.15, 1.05, 1.25, region);
        m_appliedContrast = true;
        break;
    case 1:
        KWindowEffects::enableBackgroundContrast(m_window, true, 1.38, 1.18, 1.85, region);
        m_appliedContrast = true;
        break;
    case 2:
        KWindowEffects::enableBackgroundContrast(m_window, true, 1.08, 0.94, 1.12, region);
        m_appliedContrast = true;
        break;
    case 3:
    default:
        if (m_appliedContrast) {
            KWindowEffects::enableBackgroundContrast(m_window, false);
            m_appliedContrast = false;
        }
        break;
    }
}

} // namespace fluctus::quick
