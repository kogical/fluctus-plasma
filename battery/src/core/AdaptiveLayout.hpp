// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace fluctus::battery::core {

/// Presentation breakpoint chosen from the applet's live container geometry.
enum class FormMode : std::uint8_t {
    /// Tiny panel bar (e.g. 20–32 logical px tall horizontal bar or narrow vertical strip).
    MicroPanel,
    /// Standard Plasma panel (33–56 logical px).
    CompactPanel,
    /// Tall/thick panel (> 56 logical px).
    LargePanel,
    /// Small desktop tile (e.g. 1x1 / compact square or thin strip on the desktop).
    DesktopTiny,
    /// Medium desktop card (hero summary + compact device list).
    DesktopCompact,
    /// Wide desktop dashboard (multi-column responsive grid of device cards).
    DesktopExpanded,
};

struct LayoutMetrics {
    FormMode mode{FormMode::DesktopCompact};
    int columns{1};
    double iconSize{20.0};
    double ringSize{64.0};
    double fontSizePx{13.0};
    double subFontSizePx{11.0};
    double padding{12.0};
    double spacing{8.0};
    double cornerRadius{16.0};
    bool showSecondaryLabel{true};
    bool stackVertically{false};
};

/// Computes resolution- and form-factor-adaptive layout metrics from logical container size.
/// All outputs are clamped to safe positive ranges so tiny panels and 4K/HiDPI screens
/// alike never clip text or collapse icons.
[[nodiscard]] constexpr LayoutMetrics computeLayoutMetrics(
    double width,
    double height,
    bool inPanel,
    bool verticalPanel
) noexcept {
    const double safeW = width > 1.0 ? width : 1.0;
    const double safeH = height > 1.0 ? height : 1.0;

    LayoutMetrics m;

    if (inPanel) {
        const double thickness = verticalPanel ? safeW : safeH;
        m.stackVertically = verticalPanel;

        if (thickness <= 32.0) {
            m.mode = FormMode::MicroPanel;
            m.iconSize = std::clamp(thickness * 0.58, 12.0, 20.0);
            m.fontSizePx = std::clamp(thickness * 0.44, 9.5, 13.0);
            m.subFontSizePx = std::clamp(thickness * 0.34, 8.5, 10.5);
            m.padding = std::clamp(thickness * 0.10, 2.0, 4.0);
            m.spacing = std::clamp(thickness * 0.14, 3.0, 6.0);
            m.cornerRadius = std::clamp(thickness * 0.30, 4.0, 10.0);
            m.showSecondaryLabel = false;
        } else if (thickness <= 56.0) {
            m.mode = FormMode::CompactPanel;
            m.iconSize = std::clamp(thickness * 0.52, 18.0, 28.0);
            m.fontSizePx = std::clamp(thickness * 0.34, 11.0, 15.0);
            m.subFontSizePx = std::clamp(thickness * 0.25, 9.5, 12.0);
            m.padding = std::clamp(thickness * 0.14, 4.0, 8.0);
            m.spacing = std::clamp(thickness * 0.16, 4.0, 8.0);
            m.cornerRadius = std::clamp(thickness * 0.28, 8.0, 14.0);
            m.showSecondaryLabel = !verticalPanel && thickness >= 44.0;
        } else {
            m.mode = FormMode::LargePanel;
            m.iconSize = std::clamp(thickness * 0.48, 26.0, 40.0);
            m.fontSizePx = std::clamp(thickness * 0.26, 13.0, 18.0);
            m.subFontSizePx = std::clamp(thickness * 0.20, 10.5, 13.5);
            m.padding = std::clamp(thickness * 0.14, 6.0, 12.0);
            m.spacing = std::clamp(thickness * 0.14, 6.0, 10.0);
            m.cornerRadius = std::clamp(thickness * 0.24, 10.0, 18.0);
            m.showSecondaryLabel = true;
        }
        return m;
    }

    // Desktop widget modes
    const double minDim = std::min(safeW, safeH);
    if (safeW < 200.0 || safeH < 130.0) {
        m.mode = FormMode::DesktopTiny;
        m.columns = 1;
        m.padding = std::clamp(minDim * 0.09, 8.0, 14.0);
        m.spacing = std::clamp(minDim * 0.06, 4.0, 8.0);
        m.ringSize = std::clamp(minDim - (m.padding * 2.0) - 24.0, 42.0, 96.0);
        m.iconSize = std::clamp(m.ringSize * 0.30, 14.0, 26.0);
        m.fontSizePx = std::clamp(m.ringSize * 0.28, 12.0, 24.0);
        m.subFontSizePx = std::clamp(minDim * 0.085, 10.0, 12.5);
        m.cornerRadius = std::clamp(minDim * 0.16, 14.0, 24.0);
        m.showSecondaryLabel = safeH >= 115.0;
        return m;
    }

    if (safeW < 380.0) {
        m.mode = FormMode::DesktopCompact;
        m.columns = 1;
        m.padding = std::clamp(minDim * 0.065, 12.0, 18.0);
        m.spacing = std::clamp(minDim * 0.04, 6.0, 12.0);
        m.ringSize = std::clamp(minDim * 0.26, 48.0, 74.0);
        m.iconSize = std::clamp(minDim * 0.09, 18.0, 26.0);
        m.fontSizePx = std::clamp(minDim * 0.085, 13.0, 18.0);
        m.subFontSizePx = std::clamp(minDim * 0.055, 10.5, 13.0);
        m.cornerRadius = std::clamp(minDim * 0.09, 16.0, 26.0);
        m.showSecondaryLabel = true;
        return m;
    }

    m.mode = FormMode::DesktopExpanded;
    const int targetCols = static_cast<int>(safeW / 185.0);
    m.columns = std::clamp(targetCols, 2, 4);
    m.padding = std::clamp(minDim * 0.055, 14.0, 22.0);
    m.spacing = std::clamp(minDim * 0.035, 8.0, 14.0);
    m.ringSize = std::clamp(minDim * 0.22, 52.0, 76.0);
    m.iconSize = std::clamp(minDim * 0.08, 20.0, 28.0);
    m.fontSizePx = std::clamp(minDim * 0.075, 14.0, 20.0);
    m.subFontSizePx = std::clamp(minDim * 0.05, 11.0, 13.5);
    m.cornerRadius = std::clamp(minDim * 0.08, 18.0, 28.0);
    m.showSecondaryLabel = true;
    return m;
}

} // namespace fluctus::battery::core
