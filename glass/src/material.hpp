// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Glass material as the effect renders it, parsed from the D-Bus parameter map
// (FluctusGlass/GlassProtocol.hpp) or from the global-mode configuration. QtCore/QtGui only.
#pragma once

#include "shapes.hpp"

#include <QColor>
#include <QVariantMap>

#include <vector>

namespace fluctus::glass {

struct Material {
    enum class Mode {
        Glass, ///< Liquid Glass
        Plain, ///< plain frosted blur (opt-out of global mode)
        None,  ///< no backdrop effect at all
    };

    Mode mode{Mode::Glass};
    qreal intensity{1.0};
    qreal radius{18.0};
    qreal inset{0.0};
    QColor tint{QColor::fromRgbF(1.0F, 1.0F, 1.0F, 0.015F)};
    qreal bezel{64.0};
    qreal refraction{60.0};
    qreal magnify{0.34};
    qreal edgeBend{0.15};
    qreal dispersion{0.74};
    qreal frost{0.24};
    qreal saturation{1.24};
    qreal brightness{1.02};
    qreal edgeShade{0.0};
    qreal grain{0.0};
    qreal highlight{0.84};
    qreal highlightWidth{1.85};
    qreal sheen{0.0};
    qreal lightAngle{-135.0};
    QColor innerBorder{QColor::fromRgbF(1.0F, 1.0F, 1.0F, 0.0F)};
    QColor outerBorder{QColor::fromRgbF(0.0F, 0.0F, 0.0F, 0.0F)};
    qreal borderWidth{1.0};
    QColor shadowColor{QColor::fromRgbF(0.0F, 0.0F, 0.0F, 0.0F)};
    qreal shadowBlur{30.0};
    qreal shadowOffsetX{0.0};
    qreal shadowOffsetY{8.0};
    qreal pointerLight{1.0};
    qreal pointerBulge{6.0};
    qreal pointerRadius{56.0};
    /// Explicit shapes (window-local logical px). Used as island selectors and as fallback geometry.
    std::vector<GlassShape> shapes;

    /// Returns `base` with every recognised key of `map` applied (values are clamped to sane ranges).
    [[nodiscard]] static Material fromVariantMap(const QVariantMap &map, const Material &base);
    [[nodiscard]] static Material fromVariantMap(const QVariantMap &map);

    /// Built-in looks: "liquid" (the defaults) and "crystal". Unknown names give the defaults.
    [[nodiscard]] static Material preset(QStringView name);

    /// Copy with `intensity` folded into the strength parameters (intensity becomes 1).
    [[nodiscard]] Material scaled() const;

    /// Logical px around a shape whose backdrop the shader may sample.
    [[nodiscard]] qreal sampleMargin() const;
    /// Logical px around a shape the shader may draw into (shadow + outer border).
    [[nodiscard]] qreal drawMargin() const;
    /// Whether the material ever needs the pointer.
    [[nodiscard]] bool reactsToPointer() const;

    [[nodiscard]] bool operator==(const Material &) const = default;
};

} // namespace fluctus::glass
