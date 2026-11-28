// SPDX-FileCopyrightText: 2010 Fredrik Höglund <fredrik@kde.org>
// SPDX-FileCopyrightText: 2011 Philipp Knechtges <philipp-dev@knechtges.com>
// SPDX-FileCopyrightText: 2018 Alex Nemeth <alex.nemeth329@gmail.com>
// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Backdrop capture, Dual Kawase chain and blur-region handling follow KWin's own Blur effect
// (plasma/kwin src/plugins/blur, GPL-2.0-or-later); the glass pass and request handling are new.

#include "glasseffect.hpp"

#include "fluctusglassconfig.h"

#include "FluctusGlass/GlassProtocol.hpp"

#include "core/colorspace.h"
#include "core/rendertarget.h"
#include "core/renderviewport.h"
#include "effect/effecthandler.h"
#include "effect/effectwindow.h"
#include "opengl/eglcontext.h"
#include "opengl/glframebuffer.h"
#include "opengl/glplatform.h"
#include "opengl/glshader.h"
#include "opengl/glshadermanager.h"
#include "opengl/gltexture.h"
#include "opengl/glvertexbuffer.h"
#include "scene/backgroundeffectitem.h"
#include "scene/borderradius.h"
#include "scene/windowitem.h"
#include "wayland/backgroundeffect_v1.h"
#include "wayland/surface.h"
#include "wayland_server.h"
#include "window.h"

#include <KConfigGroup>
#include <KDecoration3/Decoration>

#include <epoxy/egl.h>

#include <QEvent>
#include <QLoggingCategory>
#include <QMatrix4x4>
#include <QWindow>

#include <algorithm>
#include <array>
#include <cmath>

Q_LOGGING_CATEGORY(FLUCTUS_GLASS, "fluctus.glass", QtWarningMsg)

static void fluctusGlassInitResources() {
    // The effect is a module loaded at runtime; register its embedded shaders explicitly.
    Q_INIT_RESOURCE(fluctusglass);
}

namespace fluctus::glass {

using namespace KWin;

namespace {

constexpr std::chrono::milliseconds kPointerFade{140};
constexpr qreal kPointerHitMargin = 24.0;
constexpr qreal kShapeInsetLimit = 64.0;

/// KWin blur's strength table (1..15 → Dual Kawase iterations + offset) and the minimum
/// expansion each iteration needs so it never samples past the captured backdrop.
struct KawaseStep {
    std::size_t iterations;
    float offset;
};
struct KawaseLevel {
    float minOffset;
    float maxOffset;
    int expandSize;
};
constexpr std::array<KawaseLevel, 4> kKawaseLevels{{
    {1.0F, 2.0F, 10},
    {2.0F, 3.0F, 20},
    {2.0F, 5.0F, 50},
    {3.0F, 8.0F, 150},
}};

const std::vector<KawaseStep> &kawaseSteps() {
    static const std::vector<KawaseStep> steps = [] {
        std::vector<KawaseStep> out;
        constexpr int kSteps = 15;
        float sum = 0.0F;
        for (const KawaseLevel &level : kKawaseLevels) {
            sum += level.maxOffset - level.minOffset;
        }
        int remaining = kSteps;
        for (std::size_t i = 0; i < kKawaseLevels.size(); ++i) {
            const float span = kKawaseLevels[i].maxOffset - kKawaseLevels[i].minOffset;
            int count = static_cast<int>(std::ceil(span / sum * static_cast<float>(kSteps)));
            remaining -= count;
            if (remaining < 0) {
                count += remaining;
            }
            for (int j = 1; j <= count; ++j) {
                out.push_back({i + 1, kKawaseLevels[i].minOffset + span / static_cast<float>(count) * static_cast<float>(j)});
            }
        }
        return out;
    }();
    return steps;
}

/// strength 1..15 (KWin units). Device-resolution captures need one more iteration on HiDPI
/// to blur as far as KWin's logical-resolution blur does.
KawaseStep kawaseForStrength(int strength, qreal scale) {
    const auto &steps = kawaseSteps();
    KawaseStep step = steps[static_cast<std::size_t>(std::clamp(strength, 1, static_cast<int>(steps.size())) - 1)];
    if (scale >= 1.5 && step.iterations < kKawaseLevels.size()) {
        ++step.iterations;
    }
    return step;
}

/// Frost 0..1 → KWin strength (0 = sharp, no blur). Eased so the light frost of Liquid Glass
/// (~0.15) stays a soft veil while Frosted / Acrylic (0.75+) reach stock blur strength.
int strengthForFrost(qreal frost) {
    if (frost < 0.01) {
        return 0;
    }
    return std::clamp(static_cast<int>(std::lround(15.0 * std::pow(frost, 1.5))), 1, 15);
}

int expandForIterations(std::size_t iterations) {
    return iterations == 0 ? 0 : kKawaseLevels[std::min(iterations, kKawaseLevels.size()) - 1].expandSize;
}

QMatrix4x4 saturationMatrix(qreal saturation) {
    QMatrix4x4 matrix;
    if (!qFuzzyCompare(saturation, 1.0)) {
        const auto rval = static_cast<float>((1.0 - saturation) * 0.2126);
        const auto gval = static_cast<float>((1.0 - saturation) * 0.7152);
        const auto bval = static_cast<float>((1.0 - saturation) * 0.0722);
        const auto s = static_cast<float>(saturation);
        matrix = QMatrix4x4(rval + s, rval, rval, 0.0F,
            gval, gval + s, gval, 0.0F,
            bval, bval, bval + s, 0.0F,
            0.0F, 0.0F, 0.0F, 1.0F);
    }
    return matrix;
}

QRectF toQRectF(const RectF &rect) {
    return QRectF(rect.x(), rect.y(), rect.width(), rect.height());
}

RectF toRectF(const QRectF &rect) {
    return RectF(rect.x(), rect.y(), rect.width(), rect.height());
}

void appendQuad(std::vector<GLVertex2D> &out, const RectF &rect, const QSizeF &texSize) {
    const auto x0 = static_cast<float>(rect.left());
    const auto y0 = static_cast<float>(rect.top());
    const auto x1 = static_cast<float>(rect.right());
    const auto y1 = static_cast<float>(rect.bottom());
    const auto w = static_cast<float>(texSize.width());
    const auto h = static_cast<float>(texSize.height());
    const float u0 = x0 / w;
    const float v0 = 1.0F - y0 / h;
    const float u1 = x1 / w;
    const float v1 = 1.0F - y1 / h;
    out.push_back({.position = QVector2D(x0, y0), .texcoord = QVector2D(u0, v0)});
    out.push_back({.position = QVector2D(x1, y1), .texcoord = QVector2D(u1, v1)});
    out.push_back({.position = QVector2D(x0, y1), .texcoord = QVector2D(u0, v1)});
    out.push_back({.position = QVector2D(x0, y0), .texcoord = QVector2D(u0, v0)});
    out.push_back({.position = QVector2D(x1, y0), .texcoord = QVector2D(u1, v0)});
    out.push_back({.position = QVector2D(x1, y1), .texcoord = QVector2D(u1, v1)});
}

/// Clips `rect` (capture-local device px) against the paint region and appends the pieces.
void appendClipped(std::vector<GLVertex2D> &out, const RectF &rect, const std::vector<RectF> &clips, bool infinite, const QSizeF &texSize) {
    if (infinite) {
        appendQuad(out, rect, texSize);
        return;
    }
    for (const RectF &clip : clips) {
        const RectF piece = clip.intersected(rect);
        if (!piece.isEmpty()) {
            appendQuad(out, piece, texSize);
        }
    }
}

QVector4D mappedColor(const QColor &color, const ColorDescription &target) {
    const QVector3D rgb = ColorDescription::sRGB->mapTo(
        QVector3D(static_cast<float>(color.redF()), static_cast<float>(color.greenF()), static_cast<float>(color.blueF())),
        target, RenderingIntent::Perceptual);
    return QVector4D(rgb, static_cast<float>(color.alphaF()));
}

bool sameClass(const EffectWindow *w, const QStringList &classes) {
    if (classes.isEmpty()) {
        return false;
    }
    const QStringList tokens = w->windowClass().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString &token : tokens) {
        for (const QString &cls : classes) {
            if (token.compare(cls.trimmed(), Qt::CaseInsensitive) == 0) {
                return true;
            }
        }
    }
    return false;
}

bool matchesWindowType(const EffectWindow *w, const QString &type) {
    if (type == QLatin1String("dock")) {
        return w->isDock();
    }
    if (type == QLatin1String("normal")) {
        return w->isNormalWindow();
    }
    if (type == QLatin1String("dialog")) {
        return w->isDialog();
    }
    if (type == QLatin1String("popup")) {
        return w->isPopupWindow() || w->isMenu() || w->isPopupMenu() || w->isDropdownMenu();
    }
    if (type == QLatin1String("tooltip")) {
        return w->isTooltip();
    }
    if (type == QLatin1String("notification")) {
        return w->isNotification() || w->isCriticalNotification();
    }
    if (type == QLatin1String("osd")) {
        return w->isOnScreenDisplay();
    }
    if (type == QLatin1String("desktop")) {
        return w->isDesktop();
    }
    return false;
}

bool hasBlurRequest(const EffectWindow *w) {
    if (const SurfaceInterface *surface = w->surface(); surface && !surface->blurRegion().isEmpty()) {
        return true;
    }
    if (const QWindow *internal = w->internalWindow()) {
        return internal->property("kwin_blur").isValid();
    }
    return false;
}

const void *glProcAddress(const char *name, void *) {
    return reinterpret_cast<const void *>(eglGetProcAddress(name));
}

LgPixelFormat toLgPixelFormat(GLenum internalFormat) {
    switch (internalFormat) {
    case GL_RGBA16F:
        return LG_PIXEL_FORMAT_RGBA16_FLOAT;
    case GL_SRGB8_ALPHA8:
        return LG_PIXEL_FORMAT_RGBA8_UNORM_SRGB;
    default:
        return LG_PIXEL_FORMAT_RGBA8_UNORM;
    }
}

LgColor toLgColor(const QVector4D &v) {
    return LgColor{v.x(), v.y(), v.z(), v.w()};
}

LgMaterial toLgMaterial(const Material &m, const ColorDescription &target) {
    LgMaterial out = lg_material_default();
    out.intensity = static_cast<float>(m.intensity);
    out.tint = toLgColor(mappedColor(m.tint, target));
    out.bezel = static_cast<float>(m.bezel);
    out.refraction = static_cast<float>(m.refraction);
    out.magnify = static_cast<float>(m.magnify);
    out.edge_bend = static_cast<float>(m.edgeBend);
    out.dispersion = static_cast<float>(m.dispersion);
    out.frost = static_cast<float>(m.frost);
    out.saturation = static_cast<float>(m.saturation);
    out.brightness = static_cast<float>(m.brightness);
    out.edge_shade = static_cast<float>(m.edgeShade);
    out.grain = static_cast<float>(m.grain);
    out.highlight = static_cast<float>(m.highlight);
    out.highlight_width = static_cast<float>(m.highlightWidth);
    out.sheen = static_cast<float>(m.sheen);
    out.light_angle_deg = static_cast<float>(m.lightAngle);
    out.inner_border = toLgColor(mappedColor(m.innerBorder, target));
    out.outer_border = toLgColor(mappedColor(m.outerBorder, target));
    out.border_width = static_cast<float>(m.borderWidth);
    out.shadow_color = toLgColor(mappedColor(m.shadowColor, target));
    out.shadow_blur = static_cast<float>(m.shadowBlur);
    out.shadow_offset_x = static_cast<float>(m.shadowOffsetX);
    out.shadow_offset_y = static_cast<float>(m.shadowOffsetY);
    out.pointer_light = static_cast<float>(m.pointerLight);
    out.pointer_bulge = static_cast<float>(m.pointerBulge);
    out.pointer_radius = static_cast<float>(m.pointerRadius);
    return out;
}

} // namespace

struct GlassEffect::Shaders {
    std::unique_ptr<GLShader> downsample;
    std::unique_ptr<GLShader> upsample;
    std::unique_ptr<GLShader> plain;
};

GlassEffect::GlassEffect()
    : m_shaders(std::make_unique<Shaders>()) {
    FluctusGlassConfig::instance(effects->config());
    fluctusGlassInitResources();
    loadShaders();
    if (!m_shaders->downsample || !m_shaders->upsample || !m_shaders->plain || !m_rustRenderer) {
        qCWarning(FLUCTUS_GLASS) << "Fluctus Glass renderer failed to initialise; the effect stays inactive";
        return;
    }

    m_service.setChangeListener([this](const QString &handle) {
        onRegistrationChanged(handle);
    });
    reconfigure(ReconfigureAll);

    connect(effects, &EffectsHandler::windowAdded, this, &GlassEffect::slotWindowAdded);
    connect(effects, &EffectsHandler::windowDeleted, this, &GlassEffect::slotWindowDeleted);
    connect(effects, &EffectsHandler::viewRemoved, this, &GlassEffect::slotViewRemoved);

    m_mouseConnection = connect(effects, &EffectsHandler::mouseChanged, this,
        [this](const QPointF &pos, const QPointF &, Qt::MouseButtons, Qt::MouseButtons, Qt::KeyboardModifiers, Qt::KeyboardModifiers) {
            slotMouseChanged(pos);
        });

    const auto windows = effects->stackingOrder();
    for (EffectWindow *w : windows) {
        slotWindowAdded(w);
    }

    m_service.start();
    m_valid = true;
}

GlassEffect::~GlassEffect() {
    m_service.setChangeListener({});
    m_service.stop();
    if (m_advertisedBlur && !effects->isEffectLoaded(QStringLiteral("blur"))) {
        waylandServer()->backgroundEffectManager()->removeBlurCapability();
    }
    effects->makeOpenGLContextCurrent();
    if (m_rustRenderer) {
        lg_gl_destroy(m_rustRenderer);
        m_rustRenderer = nullptr;
    }
    m_windows.clear();
    m_shaders.reset();
}

bool GlassEffect::supported() {
    return effects->isOpenGLCompositing();
}

bool GlassEffect::enabledByDefault() {
    return false;
}

void GlassEffect::loadShaders() {
    ShaderManager *manager = ShaderManager::instance();
    const QString base = QStringLiteral(":/effects/fluctusglass/shaders/");
    m_shaders->downsample = manager->generateShaderFromFile(ShaderTrait::MapTexture, base + QStringLiteral("vertex.vert"), base + QStringLiteral("downsample.frag"));
    m_shaders->upsample = manager->generateShaderFromFile(ShaderTrait::MapTexture, base + QStringLiteral("vertex.vert"), base + QStringLiteral("upsample.frag"));
    m_shaders->plain = manager->generateShaderFromFile(ShaderTrait::MapTexture, base + QStringLiteral("vertex.vert"), base + QStringLiteral("plain.frag"));
    effects->makeOpenGLContextCurrent();
    if (const LgStatus st = lg_gl_create(&glProcAddress, nullptr, &m_rustRenderer); st != LG_STATUS_OK) {
        qCWarning(FLUCTUS_GLASS) << "Rust liquidglass GL renderer init failed with status" << static_cast<int>(st);
        m_rustRenderer = nullptr;
    }
}

void GlassEffect::reconfigure(ReconfigureFlags) {
    FluctusGlassConfig *config = FluctusGlassConfig::self();
    config->read();

    m_globalMode = config->globalMode();
    m_globalWindows = config->globalWindows();
    m_globalDecorations = config->globalDecorations();
    m_globalMenus = config->globalMenus();
    m_globalPanels = config->globalPanels();
    m_globalNotifications = config->globalNotifications();
    m_excludedClasses = config->excludedClasses().split(QLatin1Char(','), Qt::SkipEmptyParts);
    m_forcedClasses = config->forcedClasses().split(QLatin1Char(','), Qt::SkipEmptyParts);
    m_windowRadius = config->windowRadius();
    m_menuRadius = config->menuRadius();
    m_panelRadius = config->panelRadius();
    m_pointerEffects = config->pointerEffects();

    Material m;
    m.refraction = config->refraction();
    m.bezel = config->bezel();
    m.magnify = config->magnify() / 100.0;
    m.dispersion = config->dispersion() / 100.0;
    m.frost = config->frost() / 100.0;
    m.saturation = config->saturation() / 100.0;
    m.brightness = config->brightness() / 100.0;
    QColor tint = config->tintColor();
    tint.setAlphaF(static_cast<float>(config->tintStrength() / 100.0));
    m.tint = tint;
    m.highlight = config->highlight() / 100.0;
    m.edgeShade = config->edgeShade() / 100.0;
    m.sheen = config->sheen() / 100.0;
    m.pointerLight = m_pointerEffects ? 0.6 : 0.0;
    m.pointerBulge = m_pointerEffects ? config->pointerBulge() : 0.0;
    m.shadowColor = Qt::transparent; // windows bring their own shadows
    m.intensity = config->intensity() / 100.0;
    m_globalMaterial = m;

    // Plain blur next to glass must look exactly like stock blur.
    const KConfigGroup blurGroup(effects->config(), QStringLiteral("Effect-blur"));
    m_plainStrength = std::clamp(blurGroup.readEntry("BlurStrength", 15), 1, 15);
    m_plainSaturation = blurGroup.readEntry("Saturation", 150) / 100.0;

    // Global mode serves every blur request, so clients must see blur as supported even when the
    // stock Blur effect is disabled.
    if (m_globalMode && !m_advertisedBlur) {
        waylandServer()->backgroundEffectManager()->addBlurCapability();
        m_advertisedBlur = true;
    } else if (!m_globalMode && m_advertisedBlur) {
        if (!effects->isEffectLoaded(QStringLiteral("blur"))) {
            waylandServer()->backgroundEffectManager()->removeBlurCapability();
        }
        m_advertisedBlur = false;
    }
    m_service.setGlobalMode(m_globalMode);

    const auto windows = effects->stackingOrder();
    for (EffectWindow *w : windows) {
        updateWindow(w);
    }
    effects->addRepaintFull();
}

// -----------------------------------------------------------------------------
// Window bookkeeping
// -----------------------------------------------------------------------------

void GlassEffect::slotWindowAdded(EffectWindow *w) {
    auto &connections = m_windowConnections[w];
    if (SurfaceInterface *surface = w->surface()) {
        connections.push_back(connect(surface, &SurfaceInterface::blurChanged, this, [this, w]() {
            updateWindow(w);
        }));
    }
    if (QWindow *internal = w->internalWindow()) {
        internal->installEventFilter(this);
    }
    // Refraction samples beyond our own window: changes there must repaint the glass too.
    connections.push_back(connect(w, &EffectWindow::windowDamaged, this, [this, w]() {
        backdropChanged(w);
    }));
    auto watchDecoration = [this, w, &connections]() {
        if (KDecoration3::Decoration *decoration = w->decoration()) {
            connections.push_back(connect(decoration, &KDecoration3::Decoration::blurRegionChanged, this, [this, w]() {
                updateWindow(w);
            }));
        }
    };
    watchDecoration();
    connections.push_back(connect(w, &EffectWindow::windowDecorationChanged, this, [this, w]() {
        if (KDecoration3::Decoration *decoration = w->decoration()) {
            m_windowConnections[w].push_back(connect(decoration, &KDecoration3::Decoration::blurRegionChanged, this, [this, w]() {
                updateWindow(w);
            }));
        }
        updateWindow(w);
    }));
    connections.push_back(connect(w, &EffectWindow::windowFrameGeometryChanged, this, [this, w](EffectWindow *, const RectF &old) {
        if (m_windows.contains(w)) {
            updateWindow(w);
        }
        backdropChanged(w, toQRectF(old));
    }));

    resolveRegistrations();
    updateWindow(w);
}

void GlassEffect::slotWindowDeleted(EffectWindow *w) {
    if (auto it = m_windowConnections.find(w); it != m_windowConnections.end()) {
        for (const auto &connection : it->second) {
            disconnect(connection);
        }
        m_windowConnections.erase(it);
    }
    const QUuid id = w->internalId();
    releaseWindow(w);
    backdropChanged(w);
    m_service.unbindWindow(id);
    resolveRegistrations();
}

void GlassEffect::slotViewRemoved(RenderView *view) {
    effects->makeOpenGLContextCurrent();
    if (m_rustRenderer) {
        lg_gl_invalidate_caches(m_rustRenderer);
    }
    for (auto &[window, state] : m_windows) {
        state.render.erase(view);
    }
}

bool GlassEffect::eventFilter(QObject *watched, QEvent *event) {
    auto *internal = qobject_cast<QWindow *>(watched);
    if (internal && event->type() == QEvent::DynamicPropertyChange) {
        const auto *pe = static_cast<QDynamicPropertyChangeEvent *>(event);
        if (pe->propertyName() == "kwin_blur") {
            if (EffectWindow *w = effects->findWindow(internal)) {
                updateWindow(w);
            }
        }
    }
    return false;
}

void GlassEffect::releaseWindow(EffectWindow *w) {
    if (auto it = m_windows.find(w); it != m_windows.end()) {
        repaintShapes(w, it->second);
        effects->makeOpenGLContextCurrent();
        m_windows.erase(it);
    }
}

bool GlassEffect::forcedClass(const EffectWindow *w) const {
    return m_globalMode && sameClass(w, m_forcedClasses) && !sameClass(w, m_excludedClasses);
}

bool GlassEffect::globallyEligible(const EffectWindow *w) const {
    if (!m_globalMode || w->isDesktop() || sameClass(w, m_excludedClasses)) {
        return false;
    }
    if (w->isDock()) {
        return m_globalPanels;
    }
    if (w->isMenu() || w->isPopupMenu() || w->isDropdownMenu() || w->isTooltip() || w->isPopupWindow()) {
        return m_globalMenus;
    }
    if (w->isNotification() || w->isCriticalNotification() || w->isOnScreenDisplay()) {
        return m_globalNotifications;
    }
    return m_globalWindows;
}

RegionF GlassEffect::blurRegion(const EffectWindow *w, const WindowState &state) const {
    RegionF region;
    if (state.content.has_value()) {
        if (state.content->isEmpty()) {
            region = w->contentsRect(); // empty region: the whole window
        } else if (w->decoration()) {
            region = state.content->translated(w->contentsRect().topLeft()) & w->contentsRect();
        } else {
            region = *state.content & w->contentsRect();
        }
    }
    return region;
}

qreal GlassEffect::globalRadius(const EffectWindow *w, const Island &island) const {
    int configured = m_windowRadius;
    if (w->isDock()) {
        configured = m_panelRadius;
    } else if (w->isMenu() || w->isPopupMenu() || w->isDropdownMenu() || w->isTooltip() || w->isPopupWindow()) {
        configured = m_menuRadius;
    }
    if (configured >= 0) {
        return configured;
    }
    // Whole-window blur on a rounded window: use the corner radius KWin knows about.
    const BorderRadius border = w->window()->borderRadius();
    if (!border.isNull() && intersectionOverUnion(island.rect, toQRectF(w->contentsRect())) > 0.9) {
        return std::max({border.topLeft(), border.topRight(), border.bottomLeft(), border.bottomRight()});
    }
    return island.radius;
}

void GlassEffect::updateWindow(EffectWindow *w) {
    if (!m_shaders || !m_rustRenderer) {
        return;
    }
    std::optional<RegionF> content;
    std::optional<RegionF> frame;
    if (SurfaceInterface *surface = w->surface(); surface && !surface->blurRegion().isEmpty()) {
        content = surface->blurRegion();
    }
    if (QWindow *internal = w->internalWindow()) {
        const QVariant property = internal->property("kwin_blur");
        if (property.isValid()) {
            content = property.value<RegionF>();
        }
    }
    if (w->decorationHasAlpha() && w->decoration() && !w->decoration()->blurRegion().isNull()) {
        const RegionF decorationRegion = RegionF(RectF(w->decoration()->rect())) - w->contentsRect();
        frame = decorationRegion.intersected(RegionF(w->decoration()->blurRegion()));
    }

    const bool forced = forcedClass(w);
    if (forced && !content.has_value()) {
        content = RegionF(); // whole window
    }

    bool bound = false;
    const QUuid id = w->internalId();
    for (const auto &[handle, reg] : m_service.registrations()) {
        if (reg.window == id) {
            bound = true;
            break;
        }
    }
    const bool global = !forced && globallyEligible(w) && (content.has_value() || frame.has_value());

    if (!bound && !global && !forced) {
        releaseWindow(w);
        return;
    }

    WindowState &state = m_windows[w];
    state.content = content;
    state.frame = frame;
    state.forced = forced;
    state.global = global;
    rebuildShapes(w, state);

    if (state.shapes.empty() && !state.takeOver) {
        releaseWindow(w); // nothing of ours: stock blur handles the window
        return;
    }

    if (!state.item) {
        state.item = std::make_unique<BackgroundEffectItem>(w->windowItem());
    }
    int expand = expandForIterations(kawaseForStrength(m_plainStrength, 1.0).iterations);
    for (const ResolvedShape &shape : state.shapes) {
        expand = std::max(expand, expandForIterations(kawaseForStrength(strengthForFrost(shape.material.frost), 1.0).iterations));
    }
    state.item->setPixelsToExpandRepaintsBelowOpaqueRegions(static_cast<uint32_t>(expand));
    state.item->setEffectBoundingRect(toRectF(state.effectBounds));
    repaintShapes(w, state);
}

void GlassEffect::rebuildShapes(EffectWindow *w, WindowState &state) {
    const QRectF oldBounds = state.effectBounds;
    state.shapes.clear();
    state.plainRects.clear();

    const RegionF content = blurRegion(w, state);
    std::vector<QRectF> contentRects;
    for (const RectF &rect : content.rects()) {
        contentRects.push_back(toQRectF(rect));
    }
    const std::vector<Island> islands = findIslands(contentRects);
    std::vector<bool> claimed(islands.size(), false);
    const QPointF clientOffset = toQRectF(w->contentsRect()).topLeft();

    state.takeOver = false;
    auto addShape = [&state, &contentRects](const QRectF &rect, qreal radius, const Material &material) {
        if (rect.width() < 1.0 || rect.height() < 1.0) {
            return;
        }
        switch (material.mode) {
        case Material::Mode::Plain:
            // Keep the request's own region rects (rounded), not its square bounds.
            {
                bool any = false;
                for (const QRectF &piece : contentRects) {
                    if (rect.adjusted(-1, -1, 1, 1).contains(piece)) {
                        state.plainRects.push_back(piece);
                        any = true;
                    }
                }
                if (!any) {
                    state.plainRects.push_back(rect);
                }
                state.takeOver = true;
            }
            return;
        case Material::Mode::None:
            state.takeOver = true; // suppress stock blur there, draw nothing
            return;
        case Material::Mode::Glass:
            break;
        }
        state.shapes.push_back(ResolvedShape{
            .rect = rect,
            .radius = std::clamp(radius, 0.0, std::min(rect.width(), rect.height()) * 0.5),
            .material = material.scaled(),
        });
    };

    // Explicit requests first, in stable handle order.
    const QUuid id = w->internalId();
    for (const auto &[handle, reg] : m_service.registrations()) {
        if (reg.window != id) {
            continue;
        }
        const Material &material = reg.material;
        const qreal inset = std::clamp(material.inset, -kShapeInsetLimit, kShapeInsetLimit);
        if (material.shapes.empty()) {
            for (std::size_t i = 0; i < islands.size(); ++i) {
                if (!claimed[i]) {
                    claimed[i] = true;
                    addShape(islands[i].rect.adjusted(-inset, -inset, inset, inset), material.radius, material);
                }
            }
            continue;
        }
        for (const GlassShape &requested : material.shapes) {
            const QRectF wanted = requested.rect.translated(clientOffset);
            const int index = matchIsland(islands, wanted.adjusted(inset, inset, -inset, -inset));
            if (index >= 0 && !claimed[static_cast<std::size_t>(index)]) {
                // Geometry from the blur region: committed with the surface, so it never lags.
                claimed[static_cast<std::size_t>(index)] = true;
                const QRectF rect = islands[static_cast<std::size_t>(index)].rect.adjusted(-inset, -inset, inset, inset);
                addShape(rect, requested.radius, material);
            } else {
                addShape(wanted, requested.radius, material);
            }
        }
    }

    // Global mode: every remaining blur island becomes glass.
    if (state.global || state.forced) {
        for (std::size_t i = 0; i < islands.size(); ++i) {
            if (!claimed[i]) {
                claimed[i] = true;
                addShape(islands[i].rect, globalRadius(w, islands[i]), m_globalMaterial);
            }
        }
    }

    // Decoration blur.
    if (state.frame.has_value() && !state.frame->isEmpty()) {
        if ((state.global || state.forced) && m_globalDecorations) {
            Material material = m_globalMaterial;
            material.pointerBulge = 0.0;
            std::vector<QRectF> frameRects;
            for (const RectF &rect : state.frame->rects()) {
                frameRects.push_back(toQRectF(rect));
            }
            // The decoration is glass as one piece; its window content is drawn on top.
            QRectF bounds;
            for (const QRectF &rect : frameRects) {
                bounds = bounds.isNull() ? rect : bounds.united(rect);
            }
            const BorderRadius border = w->window()->borderRadius();
            const qreal radius = border.isNull() ? 0.0 : std::max(border.topLeft(), border.topRight());
            addShape(bounds, radius, material);
        } else {
            for (const RectF &rect : state.frame->rects()) {
                state.plainRects.push_back(toQRectF(rect));
            }
        }
    }

    // Whatever blur is left keeps a stock-looking frosted blur (we take over the whole window).
    // Islands claimed by a request (any mode) are handled above.
    for (const QRectF &rect : contentRects) {
        bool covered = false;
        for (std::size_t i = 0; i < islands.size() && !covered; ++i) {
            covered = claimed[i] && islands[i].rect.adjusted(-1, -1, 1, 1).contains(rect);
        }
        for (const ResolvedShape &shape : state.shapes) {
            covered = covered || shape.rect.intersects(rect);
        }
        if (!covered) {
            state.plainRects.push_back(rect);
        }
    }
    if (state.shapes.empty() && !state.takeOver) {
        state.plainRects.clear(); // nothing of ours: leave the window to stock blur
    }

    // Capture + draw extent, clamped to the window (KWin clamps the item to it anyway).
    QRectF bounds;
    const qreal plainExpand = expandForIterations(kawaseForStrength(m_plainStrength, 1.0).iterations);
    for (const ResolvedShape &shape : state.shapes) {
        const qreal frostExpand = expandForIterations(kawaseForStrength(strengthForFrost(shape.material.frost), 1.0).iterations);
        const qreal margin = std::max(shape.material.sampleMargin() + frostExpand, shape.material.drawMargin());
        const QRectF grown = shape.rect.adjusted(-margin, -margin, margin, margin);
        bounds = bounds.isNull() ? grown : bounds.united(grown);
    }
    for (const QRectF &rect : state.plainRects) {
        const QRectF grown = rect.adjusted(-plainExpand, -plainExpand, plainExpand, plainExpand);
        bounds = bounds.isNull() ? grown : bounds.united(grown);
    }
    QRectF sample;
    for (const ResolvedShape &shape : state.shapes) {
        const qreal frostExpand = expandForIterations(kawaseForStrength(strengthForFrost(shape.material.frost), 1.0).iterations);
        const qreal margin = shape.material.sampleMargin() + frostExpand;
        const QRectF grown = shape.rect.adjusted(-margin, -margin, margin, margin);
        sample = sample.isNull() ? grown : sample.united(grown);
    }
    state.sampleBounds = sample;
    const QSizeF size = toQRectF(w->frameGeometry()).size();
    state.effectBounds = bounds.intersected(QRectF(QPointF(0, 0), size));
    if (state.effectBounds != oldBounds && !oldBounds.isEmpty()) {
        effects->addRepaint(toRectF(oldBounds.translated(w->pos())));
    }
}

void GlassEffect::repaintShapes(EffectWindow *w, const WindowState &state) {
    if (!state.effectBounds.isEmpty()) {
        effects->addRepaint(toRectF(state.effectBounds.translated(w->pos())));
    }
}

// -----------------------------------------------------------------------------
// Registrations
// -----------------------------------------------------------------------------

int GlassEffect::scoreCandidate(const EffectWindow *w, const Registration &reg) const {
    namespace m = protocol::match;
    if (w->isDeleted()) {
        return -1;
    }
    const QString uuid = reg.match.value(QString(m::kWindowUuid)).toString();
    if (!uuid.isEmpty()) {
        // Even with a UUID the window must belong to the caller.
        return (w->internalId() == QUuid::fromString(uuid) && reg.pid && w->pid() == *reg.pid) ? 100 : -1;
    }
    if (!reg.pid || w->pid() != *reg.pid) {
        return -1;
    }
    int score = 0;
    if (hasBlurRequest(w)) {
        score += 4;
    }
    const auto width = reg.match.value(QString(m::kWidth));
    const auto height = reg.match.value(QString(m::kHeight));
    if (width.isValid() && height.isValid()) {
        const QSizeF frameSize = toQRectF(w->frameGeometry()).size();
        const QSizeF clientSize = toQRectF(w->contentsRect()).size();
        auto close = [&](const QSizeF &size) {
            return std::abs(size.width() - width.toDouble()) <= 1.5 && std::abs(size.height() - height.toDouble()) <= 1.5;
        };
        score += (close(frameSize) || close(clientSize)) ? 8 : -4;
    }
    const QString type = reg.match.value(QString(m::kWindowType)).toString();
    if (!type.isEmpty()) {
        score += matchesWindowType(w, type) ? 3 : -3;
    }
    const QString appId = reg.match.value(QString(m::kAppId)).toString();
    if (!appId.isEmpty() && sameClass(w, {appId})) {
        score += 2;
    }
    const QString tag = reg.match.value(QString(m::kTag)).toString();
    if (!tag.isEmpty()) {
        score += w->tag() == tag ? 3 : -3;
    }
    const QString title = reg.match.value(QString(m::kTitle)).toString();
    if (!title.isEmpty() && w->caption() == title) {
        score += 1;
    }
    return score;
}

void GlassEffect::resolveRegistrations() {
    constexpr int kMinimumScore = 4;
    QStringList handles;
    for (const auto &[handle, reg] : m_service.registrations()) {
        if (reg.window.isNull() && reg.pid) {
            handles.append(handle);
        }
    }
    if (handles.isEmpty()) {
        return;
    }
    const auto windows = effects->stackingOrder();
    for (const QString &handle : std::as_const(handles)) {
        const Registration *reg = m_service.find(handle);
        if (!reg) {
            continue;
        }
        EffectWindow *best = nullptr;
        int bestScore = kMinimumScore - 1;
        for (EffectWindow *w : windows) {
            const int score = scoreCandidate(w, *reg);
            if (score > bestScore) {
                bestScore = score;
                best = w;
            }
        }
        if (best) {
            m_service.bind(handle, best->internalId());
            updateWindow(best);
        }
    }
}

void GlassEffect::onRegistrationChanged(const QString &handle) {
    const Registration *reg = m_service.find(handle);
    if (reg && !reg->window.isNull()) {
        if (EffectWindow *w = effects->findWindow(reg->window)) {
            updateWindow(w);
            return;
        }
        m_service.unbind(handle);
    }
    resolveRegistrations();
    // A removed registration may have been the reason a window was ours.
    std::vector<EffectWindow *> windows;
    windows.reserve(m_windows.size());
    for (const auto &[w, state] : m_windows) {
        windows.push_back(w);
    }
    for (EffectWindow *w : windows) {
        updateWindow(w);
    }
}

// -----------------------------------------------------------------------------
// Pointer
// -----------------------------------------------------------------------------

void GlassEffect::slotMouseChanged(const QPointF &pos) {
    for (auto &[w, state] : m_windows) {
        bool reactive = false;
        bool inside = false;
        const QPointF local = pos - w->pos();
        for (const ResolvedShape &shape : state.shapes) {
            if (!shape.material.reactsToPointer()) {
                continue;
            }
            reactive = true;
            if (shape.rect.adjusted(-kPointerHitMargin, -kPointerHitMargin, kPointerHitMargin, kPointerHitMargin).contains(local)) {
                inside = true;
            }
        }
        if (!reactive || w->isMinimized() || !w->isOnCurrentDesktop()) {
            continue;
        }
        const qreal target = inside ? 1.0 : 0.0;
        const bool moved = inside && local != state.pointer;
        if (inside) {
            state.pointer = local;
        }
        if (target != state.pointerTarget || moved) {
            state.pointerTarget = target;
            if (m_lastFrame == std::chrono::steady_clock::time_point{}) {
                m_lastFrame = std::chrono::steady_clock::now();
            }
            repaintShapes(w, state);
        }
    }
}

// -----------------------------------------------------------------------------
// Painting
// -----------------------------------------------------------------------------

void GlassEffect::prePaintScreen(ScreenPrePaintData &data) {
    m_currentView = data.view;
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = m_lastFrame == std::chrono::steady_clock::time_point{}
        ? std::chrono::steady_clock::duration::zero()
        : std::min<std::chrono::steady_clock::duration>(now - m_lastFrame, std::chrono::milliseconds(100));
    m_lastFrame = now;
    const qreal step = std::chrono::duration<qreal>(elapsed) / std::chrono::duration<qreal>(kPointerFade);
    for (auto &[w, state] : m_windows) {
        if (state.pointerStrength < state.pointerTarget) {
            state.pointerStrength = std::min(state.pointerTarget, state.pointerStrength + step);
        } else if (state.pointerStrength > state.pointerTarget) {
            state.pointerStrength = std::max(state.pointerTarget, state.pointerStrength - step);
        }
    }
    effects->prePaintScreen(data);
}

bool GlassEffect::paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const Region &deviceRegion, LogicalOutput *screen) {
    m_frameDamage = deviceRegion;
    return effects->paintScreen(renderTarget, viewport, mask, deviceRegion, screen);
}

void GlassEffect::backdropChanged(EffectWindow *other, const QRectF &extraArea) {
    if (m_windows.empty() || !other) {
        return;
    }
    const QRectF area = toQRectF(other->expandedGeometry()).united(extraArea);
    for (auto &[w, state] : m_windows) {
        if (w == other || state.sampleBounds.isEmpty()) {
            continue;
        }
        const QRectF sample = state.sampleBounds.translated(w->pos());
        const QRectF window = toQRectF(w->frameGeometry());
        // Inside our own window KWin already repaints us (BackgroundEffectItem); only react to
        // changes in the margin the refraction reaches beyond it.
        if (area.intersects(sample) && !window.contains(area.intersected(sample))) {
            repaintShapes(w, state);
        }
    }
}

void GlassEffect::postPaintScreen() {
    bool animating = false;
    for (auto &[w, state] : m_windows) {
        if (state.pointerStrength != state.pointerTarget) {
            animating = true;
            repaintShapes(w, state);
        }
    }
    if (!animating) {
        // Idle: forget the clock so the next fade starts from a fresh timestamp.
        m_lastFrame = {};
    }
    effects->postPaintScreen();
}

bool GlassEffect::shouldRender(const EffectWindow *w, int mask, const WindowPaintData &data) const {
    if (effects->activeFullScreenEffect() && !w->data(WindowForceBlurRole).toBool()) {
        return false;
    }
    if (w->isDesktop()) {
        return false;
    }
    const bool scaled = !qFuzzyCompare(data.xScale(), 1.0) || !qFuzzyCompare(data.yScale(), 1.0);
    const bool translated = data.xTranslation() != 0.0 || data.yTranslation() != 0.0;
    // Slides (pure translation) are rendered; any other transform falls back like stock blur.
    if ((scaled || ((mask & PAINT_WINDOW_TRANSFORMED) && !translated)) && !w->data(WindowForceBlurRole).toBool()) {
        return false;
    }
    return w->opacity() * data.opacity() > 0.0;
}

bool GlassEffect::drawWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const Region &deviceRegion, WindowPaintData &data) {
    auto it = m_windows.find(w);
    if (it != m_windows.end() && shouldRender(w, mask, data)
        && render(renderTarget, viewport, w, it->second, deviceRegion, data)) {
        // We drew the backdrop for this window: make stock blur (and forks of it) skip it.
        return effects->drawWindow(renderTarget, viewport, w, mask | PAINT_WINDOW_TRANSFORMED, deviceRegion, data);
    }
    return effects->drawWindow(renderTarget, viewport, w, mask, deviceRegion, data);
}

bool GlassEffect::ensureCapture(RenderData &data, const QSize &size, unsigned int format) {
    if (data.capture && data.capture->size() == size && data.capture->internalFormat() == format) {
        return true;
    }
    data.plain = {};
    data.captureFramebuffer.reset();
    data.capture = GLTexture::allocate(format, size);
    if (!data.capture) {
        return false;
    }
    data.capture->setFilter(GL_LINEAR);
    data.capture->setWrapMode(GL_CLAMP_TO_EDGE);
    data.captureFramebuffer = std::make_unique<GLFramebuffer>(data.capture.get());
    if (!data.captureFramebuffer->valid()) {
        data.captureFramebuffer.reset();
        data.capture.reset();
        return false;
    }
    EglContext::currentContext()->pushFramebuffer(data.captureFramebuffer.get());
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    EglContext::currentContext()->popFramebuffer();
    return true;
}

bool GlassEffect::ensureChain(BlurChain &chain, const QSize &size, std::size_t iterations, unsigned int format) {
    if (chain.textures.size() == iterations
        && (iterations == 0 || (chain.textures[0]->size() == size / 2 && chain.textures[0]->internalFormat() == format))) {
        return true;
    }
    chain = {};
    for (std::size_t i = 1; i <= iterations; ++i) {
        const QSize levelSize = (size / (1 << i)).expandedTo(QSize(1, 1));
        auto texture = GLTexture::allocate(format, levelSize);
        if (!texture) {
            chain = {};
            return false;
        }
        texture->setFilter(GL_LINEAR);
        texture->setWrapMode(GL_CLAMP_TO_EDGE);
        auto framebuffer = std::make_unique<GLFramebuffer>(texture.get());
        if (!framebuffer->valid()) {
            chain = {};
            return false;
        }
        chain.textures.push_back(std::move(texture));
        chain.framebuffers.push_back(std::move(framebuffer));
    }
    return true;
}

void GlassEffect::runDualKawase(RenderData &data, BlurChain &chain, GLVertexBuffer *vbo, float offset) {
    if (chain.framebuffers.empty()) {
        return;
    }
    std::vector<GLFramebuffer *> fbos;
    fbos.push_back(data.captureFramebuffer.get());
    for (const auto &fbo : chain.framebuffers) {
        fbos.push_back(fbo.get());
    }
    const QSize size = data.capture->size();
    QMatrix4x4 projection;
    projection.ortho(QRectF(0.0, 0.0, size.width(), size.height()));

    ShaderManager::instance()->pushShader(m_shaders->downsample.get());
    m_shaders->downsample->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, projection);
    m_shaders->downsample->setUniform("offset", offset);
    for (std::size_t i = 1; i < fbos.size(); ++i) {
        GLTexture *read = fbos[i - 1]->colorAttachment();
        m_shaders->downsample->setUniform("halfpixel", QVector2D(0.5F / static_cast<float>(read->width()), 0.5F / static_cast<float>(read->height())));
        read->bind();
        GLFramebuffer::pushFramebuffer(fbos[i]);
        vbo->draw(GL_TRIANGLES, 0, 6);
    }
    ShaderManager::instance()->popShader();

    ShaderManager::instance()->pushShader(m_shaders->upsample.get());
    m_shaders->upsample->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, projection);
    m_shaders->upsample->setUniform("offset", offset);
    for (std::size_t i = fbos.size() - 1; i > 1; --i) {
        GLFramebuffer::popFramebuffer();
        GLTexture *read = fbos[i]->colorAttachment();
        m_shaders->upsample->setUniform("halfpixel", QVector2D(0.5F / static_cast<float>(read->width()), 0.5F / static_cast<float>(read->height())));
        read->bind();
        vbo->draw(GL_TRIANGLES, 0, 6);
    }
    ShaderManager::instance()->popShader();
    GLFramebuffer::popFramebuffer(); // level 1 stays the result
}

bool GlassEffect::render(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, WindowState &state, const Region &deviceRegion, WindowPaintData &data) {
    const qreal scale = viewport.scale();
    const QPointF origin = w->pos() + QPointF(data.xTranslation(), data.yTranslation());
    const QRectF item = state.effectBounds.translated(origin).intersected(toQRectF(viewport.renderRect()));
    if (item.isEmpty()) {
        return false;
    }

    // Logical capture rect: everything the shaders may sample, clamped to what KWin keeps fresh.
    const std::size_t plainIterations = kawaseForStrength(m_plainStrength, scale).iterations;
    int glassStrength = 0;
    for (const ResolvedShape &shape : state.shapes) {
        glassStrength = std::max(glassStrength, strengthForFrost(shape.material.frost));
    }
    const KawaseStep glassStep = glassStrength > 0 ? kawaseForStrength(glassStrength, scale) : KawaseStep{0, 0.0F};
    const KawaseStep plainStep = kawaseForStrength(m_plainStrength, scale);
    QRectF wanted;
    for (const ResolvedShape &shape : state.shapes) {
        const qreal margin = shape.material.sampleMargin() + expandForIterations(glassStep.iterations) / scale;
        const QRectF grown = shape.rect.translated(origin).adjusted(-margin, -margin, margin, margin);
        wanted = wanted.isNull() ? grown : wanted.united(grown);
    }
    for (const QRectF &rect : state.plainRects) {
        const qreal margin = expandForIterations(plainIterations) / scale;
        const QRectF grown = rect.translated(origin).adjusted(-margin, -margin, margin, margin);
        wanted = wanted.isNull() ? grown : wanted.united(grown);
    }
    // Capture beyond our window too (deep refraction); backdropChanged() keeps that area fresh.
    const Rect captureRect = toRectF(wanted.intersected(toQRectF(viewport.renderRect()))).roundedOut();
    if (captureRect.isEmpty()) {
        return false;
    }
    const Rect scaledCapture = captureRect.scaled(scale).rounded();
    const Rect deviceCapture = viewport.mapToDeviceCoordinates(RectF(captureRect)).rounded();
    const QSize texSize = scaledCapture.size();
    if (texSize.isEmpty()) {
        return false;
    }

    // Paint-region clip in capture-local device px (same convention as KWin's blur).
    const bool infinite = deviceRegion == Region::infinite();
    std::vector<RectF> clips;
    if (!infinite) {
        for (const Rect &clip : deviceRegion.rects()) {
            const RectF local = RectF(clip).translated(-deviceCapture.x(), -deviceCapture.y());
            if (!local.intersected(RectF(0, 0, texSize.width(), texSize.height())).isEmpty()) {
                clips.push_back(local);
            }
        }
        if (clips.empty()) {
            return true; // nothing of ours is repainted, but keep stock blur off this window
        }
    }

    GLenum format = GL_RGBA8;
    if (renderTarget.texture()) {
        format = renderTarget.texture()->internalFormat();
    }
    RenderData &rd = state.render[m_currentView];
    if (!ensureCapture(rd, texSize, format)
        || !ensureChain(rd.plain, texSize, state.plainRects.empty() ? 0 : plainStep.iterations, format)) {
        qCWarning(FLUCTUS_GLASS) << "Failed to allocate offscreen textures";
        state.render.erase(m_currentView);
        return false;
    }

    // Fetch the backdrop behind us. Everything repainted this frame (not just our own paint
    // region) already holds the windows below us; the rest of the capture is the cache from
    // earlier frames, kept fresh by BackgroundEffectItem (inside the window) and backdropChanged().
    const Region dirty = viewport.mapFromDeviceCoordinatesContained(m_frameDamage) & captureRect;
    for (const Rect &dirtyRect : dirty.rects()) {
        const Rect destination = dirtyRect.scaled(scale).rounded().translated(-scaledCapture.x(), -scaledCapture.y());
        rd.captureFramebuffer->blitFromRenderTarget(renderTarget, viewport, dirtyRect, destination);
    }

    const QSizeF texSizeF(texSize);
    const QPointF captureOrigin(scaledCapture.x(), scaledCapture.y());
    auto toLocal = [&](const QRectF &logical) {
        const QRectF device(logical.x() * scale - captureOrigin.x(), logical.y() * scale - captureOrigin.y(),
            logical.width() * scale, logical.height() * scale);
        return toRectF(device.intersected(QRectF(QPointF(0, 0), texSizeF)));
    };
    const QRectF itemClip = item;
    const auto opacity = static_cast<float>(w->opacity() * data.opacity());
    QMatrix4x4 projection = viewport.projectionMatrix();
    projection.translate(static_cast<float>(scaledCapture.x()), static_cast<float>(scaledCapture.y()));

    // Plain frosted blur (stock look) for the rest of the blur region.
    if (!state.plainRects.empty() && !rd.plain.textures.empty()) {
        std::vector<GLVertex2D> vertices;
        appendQuad(vertices, RectF(0, 0, texSize.width(), texSize.height()), texSizeF);
        const int plainStart = static_cast<int>(vertices.size());
        for (const QRectF &rect : state.plainRects) {
            appendClipped(vertices, toLocal(rect.translated(origin).intersected(itemClip)), clips, infinite, texSizeF);
        }
        const int plainCount = static_cast<int>(vertices.size()) - plainStart;
        if (plainCount > 0) {
            GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
            vbo->reset();
            vbo->setAttribLayout(std::span(GLVertexBuffer::GLVertex2DLayout), sizeof(GLVertex2D));
            if (auto mapped = vbo->map<GLVertex2D>(vertices.size())) {
                std::copy(vertices.begin(), vertices.end(), mapped->begin());
                vbo->unmap();
            } else {
                qCWarning(FLUCTUS_GLASS) << "Failed to map vertex buffer";
                return false;
            }
            vbo->bindArrays();

            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            runDualKawase(rd, rd.plain, vbo, plainStep.offset);
            GLShader *shader = m_shaders->plain.get();
            ShaderManager::instance()->pushShader(shader);
            GLTexture *read = rd.plain.textures[0].get();
            shader->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, projection);
            shader->setUniform("colorMatrix", saturationMatrix(m_plainSaturation));
            shader->setUniform("halfpixel", QVector2D(0.5F / static_cast<float>(read->width()), 0.5F / static_cast<float>(read->height())));
            shader->setUniform("offset", plainStep.offset);
            shader->setUniform("opacity", opacity);
            read->bind();
            vbo->draw(GL_TRIANGLES, plainStart, plainCount);
            ShaderManager::instance()->popShader();
            glDisable(GL_BLEND);
            vbo->unbindArrays();
        }
    }

    // Zero-copy Liquid Glass pass via standalone Rust `liquidglass` staticlib (`lg_gl_render`).
    if (!state.shapes.empty() && m_rustRenderer) {
        const ColorDescription &targetDesc = *renderTarget.colorDescription();
        const QVector3D white = mappedColor(Qt::white, targetDesc).toVector3D();
        const auto s = static_cast<float>(scale);

        m_scratchShapes.clear();
        m_scratchShapes.reserve(state.shapes.size());
        for (const ResolvedShape &shape : state.shapes) {
            const QRectF global = shape.rect.translated(origin);
            const QRectF local(global.x() * scale - captureOrigin.x(), global.y() * scale - captureOrigin.y(),
                global.width() * scale, global.height() * scale);
            const QPointF pointer = (state.pointer - shape.rect.topLeft()) * scale;
            m_scratchShapes.push_back(LgShape{
                .rect = LgRect{
                    .x = static_cast<float>(local.x()),
                    .y = static_cast<float>(local.y()),
                    .width = static_cast<float>(local.width()),
                    .height = static_cast<float>(local.height()),
                },
                .radius = static_cast<float>(shape.radius) * s,
                .opacity = opacity,
                .scale = s,
                .material = toLgMaterial(shape.material, targetDesc),
                .pointer = LgPointerState{
                    .x = static_cast<float>(pointer.x()),
                    .y = static_cast<float>(pointer.y()),
                    .strength = static_cast<float>(state.pointerStrength),
                },
            });
        }

        // Clip glass quads to the window's effect bounds intersected with KWin's damage region.
        const RectF itemLocal = toLocal(itemClip);
        m_scratchClips.clear();
        if (infinite) {
            if (!itemLocal.isEmpty()) {
                m_scratchClips.push_back(LgRect{
                    .x = static_cast<float>(itemLocal.x()),
                    .y = static_cast<float>(itemLocal.y()),
                    .width = static_cast<float>(itemLocal.width()),
                    .height = static_cast<float>(itemLocal.height()),
                });
            }
        } else {
            for (const RectF &clip : clips) {
                const RectF piece = clip.intersected(itemLocal);
                if (!piece.isEmpty()) {
                    m_scratchClips.push_back(LgRect{
                        .x = static_cast<float>(piece.x()),
                        .y = static_cast<float>(piece.y()),
                        .width = static_cast<float>(piece.width()),
                        .height = static_cast<float>(piece.height()),
                    });
                }
            }
        }

        if (!m_scratchClips.empty()) {
            GLint currentDrawFbo = 0;
            glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &currentDrawFbo);
            const uint32_t targetFbo = renderTarget.framebuffer()
                ? renderTarget.framebuffer()->handle()
                : static_cast<uint32_t>(currentDrawFbo);
            const LgPixelFormat lgFmt = toLgPixelFormat(format);

            LgRenderParams params{};
            params.backdrop = LgBackdropDesc{
                .texture_id = rd.capture->texture(),
                .width = static_cast<uint32_t>(texSize.width()),
                .height = static_cast<uint32_t>(texSize.height()),
                .uv_rect = {0.0F, 0.0F, 1.0F, 1.0F},
                .y_origin = LG_Y_ORIGIN_BOTTOM_LEFT,
                .format = lgFmt,
            };
            params.target = LgTargetDesc{
                .fbo_id = targetFbo,
                .viewport = LgRectI{0, 0, 0, 0}, // preserve KWin's active GL viewport
                .projection = {},
                .use_custom_projection = 1,
                .premultiplied_output = 1,
                .y_origin = LG_Y_ORIGIN_BOTTOM_LEFT,
                .format = lgFmt,
                .reference_white = {white.x(), white.y(), white.z()},
                .clip_rects = m_scratchClips.data(),
                .clip_rect_count = static_cast<uint32_t>(m_scratchClips.size()),
            };
            std::copy_n(projection.constData(), 16, params.target.projection);
            params.shapes = m_scratchShapes.data();
            params.shape_count = static_cast<uint32_t>(m_scratchShapes.size());

            if (const LgStatus st = lg_gl_render(m_rustRenderer, &params); st != LG_STATUS_OK) {
                qCWarning(FLUCTUS_GLASS) << "lg_gl_render failed with status" << static_cast<int>(st);
                return false;
            }
        }
    }

    return true;
}

QString GlassEffect::debug(const QString &parameter) const {
    // qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.debug fluctusglass "pointer 640 400"
    // simulates the pointer (for headless tests); "state" dumps the tracked windows.
    const QStringList args = parameter.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (args.size() == 3 && args[0] == QLatin1String("pointer")) {
        const_cast<GlassEffect *>(this)->slotMouseChanged(QPointF(args[1].toDouble(), args[2].toDouble()));
        return QStringLiteral("ok");
    }
    QString out;
    for (const auto &[w, state] : m_windows) {
        out += QStringLiteral("%1 shapes=%2 plain=%3 global=%4 pointer=%5\n")
                   .arg(w->windowClass())
                   .arg(state.shapes.size())
                   .arg(state.plainRects.size())
                   .arg(state.global)
                   .arg(state.pointerStrength);
        for (const ResolvedShape &shape : state.shapes) {
            out += QStringLiteral("  rect=%1,%2 %3x%4 radius=%5 frost=%6\n")
                       .arg(shape.rect.x()).arg(shape.rect.y()).arg(shape.rect.width()).arg(shape.rect.height())
                       .arg(shape.radius).arg(shape.material.frost);
        }
    }
    return out.isEmpty() ? QStringLiteral("no glass windows") : out;
}

bool GlassEffect::isActive() const {
    return m_valid && !m_windows.empty() && !effects->isScreenLocked();
}

bool GlassEffect::blocksDirectScanout() const {
    return false;
}

bool GlassEffect::provides(Feature feature) {
    // In global mode we are the system blur: let fullscreen effects treat us as such.
    if (feature == Blur && m_globalMode) {
        return true;
    }
    return Effect::provides(feature);
}

} // namespace fluctus::glass
