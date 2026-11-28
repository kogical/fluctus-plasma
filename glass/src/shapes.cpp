// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: GPL-3.0-or-later

#include "shapes.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <numeric>

namespace fluctus::glass {

namespace {

constexpr qreal kTouchTolerance = 0.5;

class DisjointSet {
public:
    explicit DisjointSet(std::size_t n)
        : m_parent(n) {
        std::iota(m_parent.begin(), m_parent.end(), std::size_t{0});
    }

    [[nodiscard]] std::size_t find(std::size_t i) {
        while (m_parent[i] != i) {
            m_parent[i] = m_parent[m_parent[i]];
            i = m_parent[i];
        }
        return i;
    }

    void unite(std::size_t a, std::size_t b) {
        a = find(a);
        b = find(b);
        if (a != b) {
            m_parent[std::max(a, b)] = std::min(a, b);
        }
    }

private:
    std::vector<std::size_t> m_parent;
};

[[nodiscard]] bool touches(const QRectF &a, const QRectF &b) {
    const QRectF grown = a.adjusted(-kTouchTolerance, -kTouchTolerance, kTouchTolerance, kTouchTolerance);
    return grown.intersects(b);
}

} // namespace

qreal intersectionOverUnion(const QRectF &a, const QRectF &b) {
    if (a.isEmpty() || b.isEmpty()) {
        return 0.0;
    }
    const QRectF inter = a.intersected(b);
    if (inter.isEmpty()) {
        return 0.0;
    }
    const qreal i = inter.width() * inter.height();
    const qreal u = a.width() * a.height() + b.width() * b.height() - i;
    return u > 0.0 ? i / u : 0.0;
}

qreal estimateCornerRadius(std::span<const QRectF> rects, const QRectF &bounds) {
    if (rects.size() <= 1 || bounds.isEmpty()) {
        return 0.0;
    }
    // Region rects are disjoint, so their areas add up. Four rounded corners of radius r remove
    // (4 - pi) * r^2 from the bounding box; invert that. Robust against the stair steps of
    // polygonised arcs, unlike looking for the first full-width band.
    qreal area = 0.0;
    for (const QRectF &r : rects) {
        area += r.width() * r.height();
    }
    const qreal missing = std::max(0.0, bounds.width() * bounds.height() - area);
    const qreal radius = std::sqrt(missing / (4.0 - std::numbers::pi));
    return std::clamp(radius, 0.0, std::min(bounds.width(), bounds.height()) * 0.5);
}

std::vector<Island> findIslands(std::span<const QRectF> rects) {
    std::vector<Island> islands;
    if (rects.empty()) {
        return islands;
    }
    DisjointSet sets(rects.size());
    for (std::size_t i = 0; i < rects.size(); ++i) {
        for (std::size_t j = i + 1; j < rects.size(); ++j) {
            if (touches(rects[i], rects[j])) {
                sets.unite(i, j);
            }
        }
    }

    std::vector<std::size_t> rootToIsland(rects.size(), rects.size());
    std::vector<std::vector<QRectF>> members;
    for (std::size_t i = 0; i < rects.size(); ++i) {
        if (rects[i].isEmpty()) {
            continue;
        }
        const std::size_t root = sets.find(i);
        if (rootToIsland[root] == rects.size()) {
            rootToIsland[root] = members.size();
            members.emplace_back();
        }
        members[rootToIsland[root]].push_back(rects[i]);
    }

    islands.reserve(members.size());
    for (const auto &group : members) {
        QRectF bounds;
        for (const QRectF &r : group) {
            bounds = bounds.isNull() ? r : bounds.united(r);
        }
        islands.push_back(Island{.rect = bounds, .radius = estimateCornerRadius(group, bounds)});
    }
    return islands;
}

int matchIsland(const std::vector<Island> &islands, const QRectF &shape, qreal minIoU) {
    int best = -1;
    qreal bestScore = minIoU;
    for (std::size_t i = 0; i < islands.size(); ++i) {
        const qreal score = intersectionOverUnion(islands[i].rect, shape);
        if (score >= bestScore) {
            bestScore = score;
            best = static_cast<int>(i);
        }
    }
    return best;
}

} // namespace fluctus::glass
