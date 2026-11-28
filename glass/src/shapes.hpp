// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Pure geometry helpers (QtCore/QtGui only, no KWin) that turn a window's blur region into
// rounded glass shapes. Unit-tested by tests/test_shapes.cpp.
#pragma once

#include <QRectF>

#include <span>
#include <vector>

namespace fluctus::glass {

/// A connected piece of a blur region.
struct Island {
    QRectF rect;      ///< bounding rect of the piece
    qreal radius{0};  ///< corner radius estimated from the region's stair-stepped corners
};

/// A rounded rect that receives glass.
struct GlassShape {
    QRectF rect;
    qreal radius{0};

    [[nodiscard]] bool operator==(const GlassShape &) const = default;
};

/// Groups touching/overlapping rects (as QRegion / KWin RegionF produce for a rounded polygon)
/// into islands and estimates each island's corner radius.
[[nodiscard]] std::vector<Island> findIslands(std::span<const QRectF> rects);

/// Corner radius of a rounded rect approximated by disjoint bands, from the area its corners
/// leave uncovered in the bounding rect.
[[nodiscard]] qreal estimateCornerRadius(std::span<const QRectF> rects, const QRectF &bounds);

/// Index of the island that best overlaps `shape` (intersection over union >= minIoU), or -1.
[[nodiscard]] int matchIsland(const std::vector<Island> &islands, const QRectF &shape, qreal minIoU = 0.3);

/// Intersection over union of two rects (0 when either is empty).
[[nodiscard]] qreal intersectionOverUnion(const QRectF &a, const QRectF &b);

} // namespace fluctus::glass
