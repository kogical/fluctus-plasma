#include "quick/BlurRegionController.hpp"

#include <KWindowEffects>
#include <QMetaMethod>
#include <QPainterPath>
#include <QPolygon>
#include <QQuickItem>
#include <QQuickWindow>

#include <algorithm>
#include <cmath>

namespace fluctus::quick {

BlurRegionController::BlurRegionController(QObject *parent)
    : QObject(parent) {
    m_applyTimer.setSingleShot(true);
    m_applyTimer.setInterval(0);
    connect(&m_applyTimer, &QTimer::timeout, this, &BlurRegionController::applyNow);
}

BlurRegionController::~BlurRegionController() {
    clearBlurOnWindow(m_window);
}

void BlurRegionController::setWindow(QWindow *window) {
    if (m_window == window) {
        return;
    }

    for (const auto &connection : std::as_const(m_panelConnections)) {
        disconnect(connection);
    }
    m_panelConnections.clear();
    m_panelRoot = nullptr;

    if (m_window) {
        disconnect(m_window, nullptr, this, nullptr);
        clearBlurOnWindow(m_window);
    }

    m_window = window;

    if (m_window) {
        connect(m_window, &QWindow::widthChanged, this, &BlurRegionController::reapply);
        connect(m_window, &QWindow::heightChanged, this, &BlurRegionController::reapply);
        connect(m_window, &QWindow::xChanged, this, &BlurRegionController::reapply);
        connect(m_window, &QWindow::yChanged, this, &BlurRegionController::reapply);
        connect(m_window, &QWindow::screenChanged, this, &BlurRegionController::reapply);
        connect(m_window, &QWindow::visibleChanged, this, &BlurRegionController::reapply);
        watchPanelRoot();
        reapply();
    }
}

void BlurRegionController::watchPanelRoot() {
    // PanelView (plasma-workspace) re-runs updateMask() — which unconditionally rewrites the
    // window's blur + contrast — whenever its root Panel.qml item changes panelMask or animates
    // floatingness. Listen to the same signals so our pill blur is re-applied afterwards.
    auto *quickWindow = qobject_cast<QQuickWindow *>(m_window.data());
    if (!quickWindow || !quickWindow->contentItem()) {
        return;
    }
    const QMetaMethod slot = metaObject()->method(metaObject()->indexOfSlot("reapply()"));
    const auto children = quickWindow->contentItem()->childItems();
    for (QQuickItem *child : children) {
        const QMetaObject *mo = child->metaObject();
        if (mo->indexOfProperty("panelMask") < 0) {
            continue;
        }
        m_panelRoot = child;
        for (const char *signature : {"panelMaskChanged()", "floatingnessChanged()", "panelOpacityChanged()"}) {
            const int idx = mo->indexOfSignal(signature);
            if (idx >= 0) {
                m_panelConnections.append(connect(child, mo->method(idx), this, slot));
            }
        }
        break;
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

void BlurRegionController::setCustomRegion(const QRegion &regionInWindow) {
    if (m_customRegion == regionInWindow) {
        return;
    }
    m_customRegion = regionInWindow;
    reapply();
}

void BlurRegionController::setContrast(const BlurContrast &contrast) {
    if (m_contrast == contrast) {
        return;
    }
    m_contrast = contrast;
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

QRegion BlurRegionController::currentRegion() const {
    if (!m_customRegion.isEmpty()) {
        return m_customRegion;
    }
    if (m_windowPillRect.isEmpty()) {
        return {};
    }
    return buildRoundedRegion(m_windowPillRect, m_cornerRadius);
}

QRegion BlurRegionController::panelBackgroundRegion() const {
    if (!m_window || !m_panelRoot) {
        return {};
    }
    // PanelView exposes Plasma::Types::BackgroundHints; 0 == NoBackground.
    const QVariant hints = m_window->property("backgroundHints");
    if (!hints.isValid() || hints.toInt() == 0) {
        return {};
    }
    const QVariant maskProperty = m_panelRoot->property("panelMask");
    if (maskProperty.typeId() != QMetaType::QRegion) {
        return {};
    }
    QRegion mask = maskProperty.value<QRegion>();
    const QPoint offset = m_panelRoot->property("floatingTranslucentItemOffset").toPoint();
    mask.translate(offset);
    return mask;
}

void BlurRegionController::clearBlurOnWindow(QWindow *window) {
    if (!window) {
        return;
    }
    if (m_appliedBlur) {
        // Hand the window back to the state PanelView would have set: blur only behind a
        // visible panel background, never a blanket disable that strips the panel's blur.
        const QRegion panelRegion = window == m_window ? panelBackgroundRegion() : QRegion();
        if (panelRegion.isEmpty()) {
            KWindowEffects::enableBlurBehind(window, false);
        } else {
            KWindowEffects::enableBlurBehind(window, true, panelRegion);
        }
        m_appliedBlur = false;
    }
    if (m_appliedContrast) {
        KWindowEffects::enableBackgroundContrast(window, false);
        m_appliedContrast = false;
    }
}

void BlurRegionController::reapply() {
    if (!m_applyTimer.isActive()) {
        m_applyTimer.start();
    }
}

void BlurRegionController::applyNow() {
    m_applyTimer.stop();
    if (!m_window || !m_window->isVisible()) {
        return;
    }

    if (!m_enabled) {
        clearBlurOnWindow(m_window);
        return;
    }

    const QRegion region = currentRegion();
    if (region.isEmpty()) {
        clearBlurOnWindow(m_window);
        return;
    }

    // PanelView::updateMask() just overwrote the window blur with its own mask; keep the
    // visible panel background blurred by blurring the union.
    KWindowEffects::enableBlurBehind(m_window, true, region.united(panelBackgroundRegion()));
    m_appliedBlur = true;

    if (m_contrast.enabled) {
        KWindowEffects::enableBackgroundContrast(
            m_window,
            true,
            m_contrast.contrast,
            m_contrast.intensity,
            m_contrast.saturation,
            region
        );
        m_appliedContrast = true;
    } else if (m_appliedContrast) {
        KWindowEffects::enableBackgroundContrast(m_window, false);
        m_appliedContrast = false;
    }
}

} // namespace fluctus::quick
