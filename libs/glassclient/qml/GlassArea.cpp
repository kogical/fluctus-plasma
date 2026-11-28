// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0

#include "GlassArea.hpp"

#include "FluctusGlass/GlassProtocol.hpp"

namespace fluctus::glass {

GlassArea::GlassArea(QQuickItem *parent)
    : QQuickItem(parent) {
    connect(&m_request, &GlassRequest::availableChanged, this, &GlassArea::availableChanged);
    connect(&m_request, &GlassRequest::activeChanged, this, &GlassArea::activeChanged);
    connect(&m_profile, &GlassProfile::parametersChanged, this, [this]() {
        if (!m_profileJson.isEmpty()) {
            applyProfile();
        }
    });
    m_request.setParameters(m_params);
}

GlassArea::~GlassArea() {
    disconnect(m_frameConnection);
}

bool GlassArea::glassEnabled() const {
    return m_glassEnabled;
}

void GlassArea::setGlassEnabled(bool enabled) {
    if (m_glassEnabled == enabled) {
        return;
    }
    m_glassEnabled = enabled;
    m_request.setEnabled(enabled && isVisible());
    Q_EMIT parametersChanged();
}

bool GlassArea::isAvailable() const {
    return m_request.isAvailable();
}

bool GlassArea::isActive() const {
    return m_request.isActive();
}

QString GlassArea::windowType() const {
    return m_windowType;
}

void GlassArea::setWindowType(const QString &type) {
    if (m_windowType == type) {
        return;
    }
    m_windowType = type;
    QVariantMap hints;
    if (!type.isEmpty()) {
        hints.insert(QString(protocol::match::kWindowType), type);
    }
    m_request.setMatchHints(hints);
    Q_EMIT parametersChanged();
}

void GlassArea::setFollowGlobal(bool follow) {
    if (m_request.followsGlobal() == follow) {
        return;
    }
    m_request.setFollowGlobal(follow);
    Q_EMIT parametersChanged();
}

void GlassArea::setProfile(const QString &json) {
    if (m_profileJson == json) {
        return;
    }
    m_profileJson = json;
    if (!m_profileJson.isEmpty()) {
        m_profile.loadJson(m_profileJson);
        applyProfile();
    } else {
        m_request.setFollowGlobal(false);
        pushParameters();
    }
    Q_EMIT profileChanged();
}

void GlassArea::applyProfile() {
    const qreal keptRadius = m_params.radius;
    m_params = m_profile.parameters();
    m_params.radius = keptRadius;
    m_request.setFollowGlobal(m_profile.followsGlobal());
    pushParameters();
}

void GlassArea::setReal(qreal &field, qreal value) {
    if (qFuzzyCompare(field + 1.0, value + 1.0)) {
        return;
    }
    field = value;
    pushParameters();
}

void GlassArea::setColor(QColor &field, const QColor &value) {
    if (field == value) {
        return;
    }
    field = value;
    pushParameters();
}

void GlassArea::pushParameters() {
    m_request.setParameters(m_params);
    m_lastRect = {}; // radius may have changed: refresh the blur region too
    updateRegion();
    Q_EMIT parametersChanged();
}

void GlassArea::itemChange(ItemChange change, const ItemChangeData &value) {
    if (change == ItemSceneChange) {
        attachWindow(value.window ? qobject_cast<QQuickWindow *>(value.window) : nullptr);
    } else if (change == ItemVisibleHasChanged) {
        m_request.setEnabled(m_glassEnabled && value.boolValue);
    }
    QQuickItem::itemChange(change, value);
}

void GlassArea::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) {
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    updateRegion();
}

void GlassArea::attachWindow(QQuickWindow *window) {
    disconnect(m_frameConnection);
    m_window = window;
    m_request.setWindow(window);
    if (m_window) {
        // Ancestors can move us without a geometryChange(); re-check once per rendered frame.
        // afterAnimating only fires while the scene renders, so this costs nothing when idle.
        m_frameConnection = connect(m_window, &QQuickWindow::afterAnimating, this, &GlassArea::updateRegion);
    }
    updateRegion();
}

void GlassArea::updateRegion() {
    if (!m_window) {
        return;
    }
    const QRectF rect = mapRectToScene(boundingRect());
    if (rect == m_lastRect) {
        return;
    }
    m_lastRect = rect;
    m_request.setRegion(rect, m_params.radius);
}

} // namespace fluctus::glass
