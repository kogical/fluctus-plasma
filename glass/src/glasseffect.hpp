// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fluctus Glass: draws Liquid Glass (real refraction of what is behind) for surfaces that
// request it over com.kogical.Fluctus.Glass1, or for every blur request in global mode.
//
// Chain position 19 runs before KWin's Blur effect (20): the clean backdrop is captured and the
// glass drawn first, then the window is passed on flagged PAINT_WINDOW_TRANSFORMED, which makes
// stock blur (and kwin-effects-glass) skip that one window. No double blur, no frame of plain blur,
// and every other window keeps stock blur. See glass/README.md for the full design.
#pragma once

#include "glassservice.hpp"
#include "material.hpp"

#include <liquidglass.h>

#include "core/region.h"
#include "effect/effect.h"

#include <QPointer>
#include <QUuid>

#include <chrono>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace KWin {
class BackgroundEffectItem;
class EffectWindow;
class GLFramebuffer;
class GLShader;
class GLTexture;
class GLVertexBuffer;
class RenderView;
} // namespace KWin

namespace fluctus::glass {

/// Dual Kawase levels 1..n (level 0 is the shared capture).
struct BlurChain {
    std::vector<std::unique_ptr<KWin::GLTexture>> textures;
    std::vector<std::unique_ptr<KWin::GLFramebuffer>> framebuffers;
};

/// Per render view (output): the sharp backdrop capture, cached between frames like KWin's blur,
/// plus the stock-strength blur chain for plain blur regions (Liquid Glass blur textures are
/// pooled inside the Rust `LgGlRenderer`).
struct RenderData {
    std::unique_ptr<KWin::GLTexture> capture;
    std::unique_ptr<KWin::GLFramebuffer> captureFramebuffer;
    BlurChain plain;
};

/// One glass shape of a window, resolved for rendering (window-local logical px).
struct ResolvedShape {
    QRectF rect;
    qreal radius{0};
    Material material;
};

struct WindowState {
    // Inputs (window-local logical px, as KWin's Blur effect computes them).
    std::optional<KWin::RegionF> content;
    std::optional<KWin::RegionF> frame;
    bool forced{false};   ///< global-mode "force glass" window class
    bool global{false};   ///< eligible for global-mode glass
    bool takeOver{false}; ///< a request disabled the backdrop effect somewhere (mode "none")

    // Derived
    std::vector<ResolvedShape> shapes;
    std::vector<QRectF> plainRects;   ///< blur region left to plain frosted blur
    QRectF effectBounds;              ///< draw extent (+ in-window capture), window-local, clamped to the window
    QRectF sampleBounds;              ///< everything the shaders may sample, window-local, unclamped
    std::unique_ptr<KWin::BackgroundEffectItem> item;
    std::unordered_map<KWin::RenderView *, RenderData> render;

    // Pointer
    QPointF pointer;           ///< window-local logical px
    qreal pointerStrength{0};
    qreal pointerTarget{0};
};

class GlassEffect : public KWin::Effect {
    Q_OBJECT

public:
    GlassEffect();
    ~GlassEffect() override;

    GlassEffect(const GlassEffect &) = delete;
    GlassEffect &operator=(const GlassEffect &) = delete;

    static bool supported();
    static bool enabledByDefault();

    void reconfigure(ReconfigureFlags flags) override;
    void prePaintScreen(KWin::ScreenPrePaintData &data) override;
    bool paintScreen(const KWin::RenderTarget &renderTarget, const KWin::RenderViewport &viewport, int mask,
        const KWin::Region &deviceRegion, KWin::LogicalOutput *screen) override;
    void postPaintScreen() override;
    bool drawWindow(const KWin::RenderTarget &renderTarget, const KWin::RenderViewport &viewport,
        KWin::EffectWindow *w, int mask, const KWin::Region &deviceRegion, KWin::WindowPaintData &data) override;

    bool isActive() const override;
    bool blocksDirectScanout() const override;
    bool provides(Feature feature) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    QString debug(const QString &parameter) const override;

    int requestedEffectChainPosition() const override {
        return 19;
    }

private:
    struct Shaders;

    void loadShaders();
    void slotWindowAdded(KWin::EffectWindow *w);
    void slotWindowDeleted(KWin::EffectWindow *w);
    void slotViewRemoved(KWin::RenderView *view);
    void slotMouseChanged(const QPointF &pos);
    /// Another window changed: repaint glass whose refraction samples where it is.
    void backdropChanged(KWin::EffectWindow *other, const QRectF &extraArea = {});
    void onRegistrationChanged(const QString &handle);

    /// Re-reads the blur region of `w` and recomputes its shapes.
    void updateWindow(KWin::EffectWindow *w);
    void rebuildShapes(KWin::EffectWindow *w, WindowState &state);
    void resolveRegistrations();
    void releaseWindow(KWin::EffectWindow *w);
    void repaintShapes(KWin::EffectWindow *w, const WindowState &state);

    [[nodiscard]] bool globallyEligible(const KWin::EffectWindow *w) const;
    [[nodiscard]] bool forcedClass(const KWin::EffectWindow *w) const;
    [[nodiscard]] KWin::RegionF blurRegion(const KWin::EffectWindow *w, const WindowState &state) const;
    [[nodiscard]] qreal globalRadius(const KWin::EffectWindow *w, const Island &island) const;
    [[nodiscard]] bool shouldRender(const KWin::EffectWindow *w, int mask, const KWin::WindowPaintData &data) const;
    [[nodiscard]] int scoreCandidate(const KWin::EffectWindow *w, const Registration &reg) const;

    bool render(const KWin::RenderTarget &renderTarget, const KWin::RenderViewport &viewport,
        KWin::EffectWindow *w, WindowState &state, const KWin::Region &deviceRegion, KWin::WindowPaintData &data);
    bool ensureCapture(RenderData &data, const QSize &size, unsigned int format);
    bool ensureChain(BlurChain &chain, const QSize &size, std::size_t iterations, unsigned int format);
    void runDualKawase(RenderData &data, BlurChain &chain, KWin::GLVertexBuffer *vbo, float offset);

    std::unique_ptr<Shaders> m_shaders;
    LgGlRenderer *m_rustRenderer{nullptr};
    std::vector<LgShape> m_scratchShapes;
    std::vector<LgRect> m_scratchClips;
    GlassService m_service;
    Material m_globalMaterial;
    std::unordered_map<KWin::EffectWindow *, WindowState> m_windows;
    std::unordered_map<KWin::EffectWindow *, std::vector<QMetaObject::Connection>> m_windowConnections;
    KWin::RenderView *m_currentView{nullptr};
    KWin::Region m_frameDamage;   ///< device region repainted this frame (valid backdrop pixels)
    std::chrono::steady_clock::time_point m_lastFrame{};
    QMetaObject::Connection m_mouseConnection;
    QStringList m_excludedClasses;
    QStringList m_forcedClasses;
    int m_windowRadius{-1};
    int m_menuRadius{-1};
    int m_panelRadius{-1};
    int m_plainStrength{10};
    qreal m_plainSaturation{1.0};
    bool m_globalMode{false};
    bool m_globalWindows{true};
    bool m_globalDecorations{true};
    bool m_globalMenus{true};
    bool m_globalPanels{true};
    bool m_globalNotifications{true};
    bool m_pointerEffects{true};
    bool m_advertisedBlur{false};
    bool m_valid{false};
};

} // namespace fluctus::glass
