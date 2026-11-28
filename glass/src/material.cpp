// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: GPL-3.0-or-later

#include "material.hpp"

#include "FluctusGlass/GlassProtocol.hpp"

#include <QVariantList>

#include <algorithm>
#include <cmath>

namespace fluctus::glass {

namespace {

namespace p = protocol::param;

void readReal(const QVariantMap &map, QLatin1StringView key, qreal &out, qreal lo, qreal hi) {
    const auto it = map.constFind(QString(key));
    if (it == map.cend()) {
        return;
    }
    bool ok = false;
    const qreal value = it->toDouble(&ok);
    if (ok && std::isfinite(value)) {
        out = std::clamp(value, lo, hi);
    }
}

void readColor(const QVariantMap &map, QLatin1StringView key, QColor &out) {
    const auto it = map.constFind(QString(key));
    if (it == map.cend()) {
        return;
    }
    QColor color;
    if (it->metaType().id() == QMetaType::QColor) {
        color = it->value<QColor>();
    } else {
        color = QColor::fromString(it->toString());
    }
    if (color.isValid()) {
        out = color;
    }
}

constexpr std::size_t kMaxShapes = 32;
constexpr qreal kMaxCoordinate = 65536.0;

void readShapes(const QVariantMap &map, std::vector<GlassShape> &out) {
    const auto it = map.constFind(QString(p::kShapes));
    if (it == map.cend()) {
        return;
    }
    out.clear();
    const QVariantList list = it->toList();
    for (qsizetype i = 0; i + 4 < list.size() && out.size() < kMaxShapes; i += 5) {
        const qreal x = list.at(i).toDouble();
        const qreal y = list.at(i + 1).toDouble();
        const qreal w = list.at(i + 2).toDouble();
        const qreal h = list.at(i + 3).toDouble();
        const qreal r = list.at(i + 4).toDouble();
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(w) || !std::isfinite(h) || !std::isfinite(r)) {
            continue;
        }
        if (w <= 0.0 || h <= 0.0 || std::abs(x) > kMaxCoordinate || std::abs(y) > kMaxCoordinate || w > kMaxCoordinate || h > kMaxCoordinate) {
            continue;
        }
        out.push_back(GlassShape{.rect = QRectF(x, y, w, h), .radius = std::clamp(r, 0.0, std::min(w, h) * 0.5)});
    }
}

} // namespace

Material Material::preset(QStringView name) {
    Material m;
    if (name == p::kPresetLight) {
        m.intensity = 0.55;
        m.tint = QColor::fromRgbF(1.0F, 1.0F, 1.0F, 0.03F);
        m.bezel = 22.0;
        m.refraction = 18.0;
        m.magnify = 0.04;
        m.edgeBend = 0.10;
        m.dispersion = 0.18;
        m.frost = 0.20;
        m.saturation = 1.15;
        m.brightness = 1.02;
        m.highlight = 0.55;
        m.sheen = 0.04;
        m.pointerBulge = 3.0;
    } else if (name == p::kPresetMedium) {
        m.intensity = 1.0;
        m.tint = QColor::fromRgbF(1.0F, 1.0F, 1.0F, 0.04F);
        m.bezel = 36.0;
        m.refraction = 32.0;
        m.magnify = 0.12;
        m.edgeBend = 0.14;
        m.dispersion = 0.40;
        m.frost = 0.18;
        m.saturation = 1.35;
        m.brightness = 1.04;
        m.highlight = 0.85;
        m.sheen = 0.05;
        m.pointerBulge = 6.0;
    } else if (name == p::kPresetFull) {
        m.intensity = 1.25;
        m.tint = QColor::fromRgbF(1.0F, 1.0F, 1.0F, 0.015F);
        m.bezel = 64.0;
        m.refraction = 60.0;
        m.magnify = 0.34;
        m.edgeBend = 0.15;
        m.dispersion = 0.74;
        m.frost = 0.24;
        m.saturation = 1.30;
        m.brightness = 1.03;
        m.highlight = 1.0;
        m.sheen = 0.04;
        m.pointerBulge = 8.0;
    } else if (name == p::kPresetCrystal) {
        m.tint = QColor::fromRgbF(1.0F, 1.0F, 1.0F, 0.02F);
        m.bezel = 40.0;
        m.refraction = 44.0;
        m.magnify = 0.04;
        m.edgeBend = 0.25;
        m.dispersion = 0.5;
        m.frost = 0.0;
        m.saturation = 1.35;
        m.highlight = 1.25;
        m.highlightWidth = 5.0;
    }
    return m;
}

QVariantMap Material::toVariantMap() const {
    QString modeStr = QString(p::kModeGlass);
    if (mode == Mode::Plain) {
        modeStr = QString(p::kModePlain);
    } else if (mode == Mode::None) {
        modeStr = QString(p::kModeNone);
    }
    return {
        {QString(p::kMode), modeStr},
        {QString(p::kIntensity), intensity},
        {QString(p::kRadius), radius},
        {QString(p::kInset), inset},
        {QString(p::kTint), tint.name(QColor::HexArgb)},
        {QString(p::kBezel), bezel},
        {QString(p::kRefraction), refraction},
        {QString(p::kMagnify), magnify},
        {QString(p::kEdgeBend), edgeBend},
        {QString(p::kDispersion), dispersion},
        {QString(p::kFrost), frost},
        {QString(p::kSaturation), saturation},
        {QString(p::kBrightness), brightness},
        {QString(p::kEdgeShade), edgeShade},
        {QString(p::kGrain), grain},
        {QString(p::kHighlight), highlight},
        {QString(p::kHighlightWidth), highlightWidth},
        {QString(p::kSheen), sheen},
        {QString(p::kLightAngle), lightAngle},
        {QString(p::kInnerBorder), innerBorder.name(QColor::HexArgb)},
        {QString(p::kOuterBorder), outerBorder.name(QColor::HexArgb)},
        {QString(p::kBorderWidth), borderWidth},
        {QString(p::kShadowColor), shadowColor.name(QColor::HexArgb)},
        {QString(p::kShadowBlur), shadowBlur},
        {QString(p::kShadowOffsetX), shadowOffsetX},
        {QString(p::kShadowOffsetY), shadowOffsetY},
        {QString(p::kPointerLight), pointerLight},
        {QString(p::kPointerBulge), pointerBulge},
        {QString(p::kPointerRadius), pointerRadius},
    };
}

Material Material::fromVariantMap(const QVariantMap &map, const Material &base) {
    Material m = base;
    if (const auto preset = map.constFind(QString(p::kPreset)); preset != map.cend()) {
        const QString presetName = preset->toString();
        if (presetName != p::kPresetGlobal) {
            const Material shapesKept = m;
            m = Material::preset(presetName);
            m.shapes = shapesKept.shapes;
        }
    }
    if (const auto mode = map.constFind(QString(p::kMode)); mode != map.cend()) {
        const QString value = mode->toString();
        if (value == p::kModePlain) {
            m.mode = Mode::Plain;
        } else if (value == p::kModeNone) {
            m.mode = Mode::None;
        } else if (value == p::kModeGlass) {
            m.mode = Mode::Glass;
        }
    }
    readReal(map, p::kIntensity, m.intensity, 0.0, 2.0);
    readReal(map, p::kRadius, m.radius, 0.0, 4096.0);
    readReal(map, p::kInset, m.inset, -64.0, 64.0);
    readColor(map, p::kTint, m.tint);
    readReal(map, p::kBezel, m.bezel, 0.0, 256.0);
    readReal(map, p::kRefraction, m.refraction, 0.0, 128.0);
    readReal(map, p::kMagnify, m.magnify, 0.0, 0.45);
    readReal(map, p::kEdgeBend, m.edgeBend, 0.0, 1.0);
    readReal(map, p::kDispersion, m.dispersion, 0.0, 1.0);
    readReal(map, p::kFrost, m.frost, 0.0, 1.0);
    readReal(map, p::kSaturation, m.saturation, 0.0, 4.0);
    readReal(map, p::kBrightness, m.brightness, 0.0, 4.0);
    readReal(map, p::kEdgeShade, m.edgeShade, 0.0, 1.0);
    readReal(map, p::kGrain, m.grain, 0.0, 1.0);
    readReal(map, p::kHighlight, m.highlight, 0.0, 4.0);
    readReal(map, p::kHighlightWidth, m.highlightWidth, 0.0, 64.0);
    readReal(map, p::kSheen, m.sheen, 0.0, 1.0);
    readReal(map, p::kLightAngle, m.lightAngle, -360.0, 360.0);
    readColor(map, p::kInnerBorder, m.innerBorder);
    readColor(map, p::kOuterBorder, m.outerBorder);
    readReal(map, p::kBorderWidth, m.borderWidth, 0.0, 16.0);
    readColor(map, p::kShadowColor, m.shadowColor);
    readReal(map, p::kShadowBlur, m.shadowBlur, 0.0, 128.0);
    readReal(map, p::kShadowOffsetX, m.shadowOffsetX, -128.0, 128.0);
    readReal(map, p::kShadowOffsetY, m.shadowOffsetY, -128.0, 128.0);
    readReal(map, p::kPointerLight, m.pointerLight, 0.0, 1.0);
    readReal(map, p::kPointerBulge, m.pointerBulge, 0.0, 64.0);
    readReal(map, p::kPointerRadius, m.pointerRadius, 1.0, 1024.0);
    readShapes(map, m.shapes);
    return m;
}

Material Material::fromVariantMap(const QVariantMap &map) {
    return fromVariantMap(map, Material{});
}

Material Material::scaled() const {
    Material m = *this;
    const qreal k = intensity;
    m.refraction = refraction * k;
    m.magnify = std::min(0.45, magnify * k);
    m.dispersion = std::min(1.0, dispersion * k);
    m.pointerBulge = pointerBulge * k;
    m.highlight = highlight * std::sqrt(k);
    m.intensity = 1.0;
    return m;
}

qreal Material::sampleMargin() const {
    // Two-zone bezel refraction (including outer lip fold), dispersion spread, and pointer bulge
    // can all sample outside the shape silhouette.
    return std::ceil(refraction * (1.75 + 0.70 * edgeBend) * (1.0 + dispersion) + pointerBulge * 1.5 + 8.0);
}

qreal Material::drawMargin() const {
    qreal margin = borderWidth + 2.0;
    if (shadowColor.alphaF() > 0.001F) {
        margin = std::max(margin, shadowBlur + std::max(std::abs(shadowOffsetX), std::abs(shadowOffsetY)));
    }
    return std::ceil(margin);
}

bool Material::reactsToPointer() const {
    return pointerLight > 0.001 || pointerBulge > 0.001;
}

} // namespace fluctus::glass
