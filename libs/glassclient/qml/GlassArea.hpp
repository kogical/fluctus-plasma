// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
//
// QML element that requests Fluctus Glass behind its own geometry:
//
//     import Kogical.FluctusGlass
//     GlassArea { anchors.fill: parent; radius: 24 }
//
// Draws nothing itself. Leave the area behind it transparent in your window.
#pragma once

#include "FluctusGlass/GlassProfile.hpp"
#include "FluctusGlass/GlassRequest.hpp"

#include <QColor>
#include <QPointer>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtQml/qqmlregistration.h>

namespace fluctus::glass {

class GlassArea : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool glassEnabled READ glassEnabled WRITE setGlassEnabled NOTIFY parametersChanged)
    Q_PROPERTY(bool available READ isAvailable NOTIFY availableChanged)
    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged)
    Q_PROPERTY(QString windowType READ windowType WRITE setWindowType NOTIFY parametersChanged)
    /// Plain (rounded) blur only; becomes glass when Fluctus Glass runs in global mode.
    Q_PROPERTY(bool blurOnly READ blurOnly WRITE setBlurOnly NOTIFY parametersChanged)
    /// When true, inherit KWin's live global Desktop Effects material instead of explicit properties.
    Q_PROPERTY(bool followGlobal READ followGlobal WRITE setFollowGlobal NOTIFY parametersChanged)
    /// Optional JSON GlassProfile string (e.g. bound to Plasmoid.configuration.glassProfile).
    /// When non-empty, drives followGlobal and all material/pointer parameters (keeping `radius`).
    Q_PROPERTY(QString profile READ profile WRITE setProfile NOTIFY profileChanged)

    /// "glass" (default), "plain" (frosted blur even in global mode) or "none" (no backdrop effect).
    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY parametersChanged)
    /// Master strength 0..2 (default 1).
    Q_PROPERTY(qreal intensity READ intensity WRITE setIntensity NOTIFY parametersChanged)
    Q_PROPERTY(qreal magnify READ magnify WRITE setMagnify NOTIFY parametersChanged)
    Q_PROPERTY(qreal radius READ radius WRITE setRadius NOTIFY parametersChanged)
    Q_PROPERTY(QColor tint READ tint WRITE setTint NOTIFY parametersChanged)
    Q_PROPERTY(qreal bezel READ bezel WRITE setBezel NOTIFY parametersChanged)
    Q_PROPERTY(qreal refraction READ refraction WRITE setRefraction NOTIFY parametersChanged)
    Q_PROPERTY(qreal dispersion READ dispersion WRITE setDispersion NOTIFY parametersChanged)
    Q_PROPERTY(qreal frost READ frost WRITE setFrost NOTIFY parametersChanged)
    Q_PROPERTY(qreal saturation READ saturation WRITE setSaturation NOTIFY parametersChanged)
    Q_PROPERTY(qreal brightness READ brightness WRITE setBrightness NOTIFY parametersChanged)
    Q_PROPERTY(qreal edgeShade READ edgeShade WRITE setEdgeShade NOTIFY parametersChanged)
    Q_PROPERTY(qreal grain READ grain WRITE setGrain NOTIFY parametersChanged)
    Q_PROPERTY(qreal highlight READ highlight WRITE setHighlight NOTIFY parametersChanged)
    Q_PROPERTY(qreal highlightWidth READ highlightWidth WRITE setHighlightWidth NOTIFY parametersChanged)
    Q_PROPERTY(qreal sheen READ sheen WRITE setSheen NOTIFY parametersChanged)
    Q_PROPERTY(qreal lightAngle READ lightAngle WRITE setLightAngle NOTIFY parametersChanged)
    Q_PROPERTY(QColor innerBorder READ innerBorder WRITE setInnerBorder NOTIFY parametersChanged)
    Q_PROPERTY(QColor outerBorder READ outerBorder WRITE setOuterBorder NOTIFY parametersChanged)
    Q_PROPERTY(qreal borderWidth READ borderWidth WRITE setBorderWidth NOTIFY parametersChanged)
    Q_PROPERTY(QColor shadowColor READ shadowColor WRITE setShadowColor NOTIFY parametersChanged)
    Q_PROPERTY(qreal shadowBlur READ shadowBlur WRITE setShadowBlur NOTIFY parametersChanged)
    Q_PROPERTY(qreal shadowOffsetX READ shadowOffsetX WRITE setShadowOffsetX NOTIFY parametersChanged)
    Q_PROPERTY(qreal shadowOffsetY READ shadowOffsetY WRITE setShadowOffsetY NOTIFY parametersChanged)
    Q_PROPERTY(qreal pointerLight READ pointerLight WRITE setPointerLight NOTIFY parametersChanged)
    Q_PROPERTY(qreal pointerBulge READ pointerBulge WRITE setPointerBulge NOTIFY parametersChanged)
    Q_PROPERTY(qreal pointerRadius READ pointerRadius WRITE setPointerRadius NOTIFY parametersChanged)

public:
    explicit GlassArea(QQuickItem *parent = nullptr);
    ~GlassArea() override;

    [[nodiscard]] bool glassEnabled() const;
    void setGlassEnabled(bool enabled);
    [[nodiscard]] bool isAvailable() const;
    [[nodiscard]] bool isActive() const;
    [[nodiscard]] QString windowType() const;
    void setWindowType(const QString &type);
    [[nodiscard]] bool blurOnly() const { return m_request.isBlurOnly(); }
    void setBlurOnly(bool blurOnly) { m_request.setBlurOnly(blurOnly); Q_EMIT parametersChanged(); }
    [[nodiscard]] bool followGlobal() const { return m_request.followsGlobal(); }
    void setFollowGlobal(bool follow);
    [[nodiscard]] QString profile() const { return m_profileJson; }
    void setProfile(const QString &json);

    [[nodiscard]] QString mode() const { return m_params.mode; }
    void setMode(const QString &value) { if (m_params.mode != value) { m_params.mode = value; pushParameters(); } }
    [[nodiscard]] qreal intensity() const { return m_params.intensity; }
    void setIntensity(qreal value) { setReal(m_params.intensity, value); }
    [[nodiscard]] qreal magnify() const { return m_params.magnify; }
    void setMagnify(qreal value) { setReal(m_params.magnify, value); }
    [[nodiscard]] qreal radius() const { return m_params.radius; }
    void setRadius(qreal value) { setReal(m_params.radius, value); }
    [[nodiscard]] QColor tint() const { return m_params.tint; }
    void setTint(const QColor &value) { setColor(m_params.tint, value); }
    [[nodiscard]] qreal bezel() const { return m_params.bezel; }
    void setBezel(qreal value) { setReal(m_params.bezel, value); }
    [[nodiscard]] qreal refraction() const { return m_params.refraction; }
    void setRefraction(qreal value) { setReal(m_params.refraction, value); }
    [[nodiscard]] qreal dispersion() const { return m_params.dispersion; }
    void setDispersion(qreal value) { setReal(m_params.dispersion, value); }
    [[nodiscard]] qreal frost() const { return m_params.frost; }
    void setFrost(qreal value) { setReal(m_params.frost, value); }
    [[nodiscard]] qreal saturation() const { return m_params.saturation; }
    void setSaturation(qreal value) { setReal(m_params.saturation, value); }
    [[nodiscard]] qreal brightness() const { return m_params.brightness; }
    void setBrightness(qreal value) { setReal(m_params.brightness, value); }
    [[nodiscard]] qreal edgeShade() const { return m_params.edgeShade; }
    void setEdgeShade(qreal value) { setReal(m_params.edgeShade, value); }
    [[nodiscard]] qreal grain() const { return m_params.grain; }
    void setGrain(qreal value) { setReal(m_params.grain, value); }
    [[nodiscard]] qreal highlight() const { return m_params.highlight; }
    void setHighlight(qreal value) { setReal(m_params.highlight, value); }
    [[nodiscard]] qreal highlightWidth() const { return m_params.highlightWidth; }
    void setHighlightWidth(qreal value) { setReal(m_params.highlightWidth, value); }
    [[nodiscard]] qreal sheen() const { return m_params.sheen; }
    void setSheen(qreal value) { setReal(m_params.sheen, value); }
    [[nodiscard]] qreal lightAngle() const { return m_params.lightAngle; }
    void setLightAngle(qreal value) { setReal(m_params.lightAngle, value); }
    [[nodiscard]] QColor innerBorder() const { return m_params.innerBorder; }
    void setInnerBorder(const QColor &value) { setColor(m_params.innerBorder, value); }
    [[nodiscard]] QColor outerBorder() const { return m_params.outerBorder; }
    void setOuterBorder(const QColor &value) { setColor(m_params.outerBorder, value); }
    [[nodiscard]] qreal borderWidth() const { return m_params.borderWidth; }
    void setBorderWidth(qreal value) { setReal(m_params.borderWidth, value); }
    [[nodiscard]] QColor shadowColor() const { return m_params.shadowColor; }
    void setShadowColor(const QColor &value) { setColor(m_params.shadowColor, value); }
    [[nodiscard]] qreal shadowBlur() const { return m_params.shadowBlur; }
    void setShadowBlur(qreal value) { setReal(m_params.shadowBlur, value); }
    [[nodiscard]] qreal shadowOffsetX() const { return m_params.shadowOffsetX; }
    void setShadowOffsetX(qreal value) { setReal(m_params.shadowOffsetX, value); }
    [[nodiscard]] qreal shadowOffsetY() const { return m_params.shadowOffsetY; }
    void setShadowOffsetY(qreal value) { setReal(m_params.shadowOffsetY, value); }
    [[nodiscard]] qreal pointerLight() const { return m_params.pointerLight; }
    void setPointerLight(qreal value) { setReal(m_params.pointerLight, value); }
    [[nodiscard]] qreal pointerBulge() const { return m_params.pointerBulge; }
    void setPointerBulge(qreal value) { setReal(m_params.pointerBulge, value); }
    [[nodiscard]] qreal pointerRadius() const { return m_params.pointerRadius; }
    void setPointerRadius(qreal value) { setReal(m_params.pointerRadius, value); }

Q_SIGNALS:
    void parametersChanged();
    void profileChanged();
    void availableChanged();
    void activeChanged();

protected:
    void itemChange(ItemChange change, const ItemChangeData &value) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private:
    void applyProfile();
    void pushParameters();
    void setReal(qreal &field, qreal value);
    void setColor(QColor &field, const QColor &value);
    void updateRegion();
    void attachWindow(QQuickWindow *window);

    GlassRequest m_request;
    GlassProfile m_profile;
    GlassParameters m_params;
    QPointer<QQuickWindow> m_window;
    QMetaObject::Connection m_frameConnection;
    QString m_windowType;
    QString m_profileJson;
    QRectF m_lastRect;
    bool m_glassEnabled{true};
};

} // namespace fluctus::glass
