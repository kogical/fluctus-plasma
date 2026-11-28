// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0

#include "FluctusGlass/GlassRequest.hpp"
#include "FluctusGlass/GlassProtocol.hpp"

#include <KWindowEffects>

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QGuiApplication>
#include <QHash>
#include <QPainterPath>
#include <QPolygon>

#include <algorithm>

namespace fluctus::glass {

namespace {

QString argb(const QColor &color) {
    return color.name(QColor::HexArgb);
}

/// Every request that manages the blur region of a window, so several GlassAreas in one window
/// share a single blur region (KWin keeps one region per surface) instead of overwriting it.
QHash<QWindow *, QList<GlassRequest *>> &blurOwners() {
    static QHash<QWindow *, QList<GlassRequest *>> owners;
    return owners;
}

QDBusMessage glassCall(const QString &method) {
    return QDBusMessage::createMethodCall(
        QString(protocol::kService),
        QString(protocol::kPath),
        QString(protocol::kInterface),
        method
    );
}

} // namespace

QVariantMap GlassParameters::toVariantMap() const {
    namespace p = protocol::param;
    return {
        {QString(p::kMode), mode},
        {QString(p::kIntensity), intensity},
        {QString(p::kRadius), radius},
        {QString(p::kInset), inset},
        {QString(p::kTint), argb(tint)},
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
        {QString(p::kInnerBorder), argb(innerBorder)},
        {QString(p::kOuterBorder), argb(outerBorder)},
        {QString(p::kBorderWidth), borderWidth},
        {QString(p::kShadowColor), argb(shadowColor)},
        {QString(p::kShadowBlur), shadowBlur},
        {QString(p::kShadowOffsetX), shadowOffsetX},
        {QString(p::kShadowOffsetY), shadowOffsetY},
        {QString(p::kPointerLight), pointerLight},
        {QString(p::kPointerBulge), pointerBulge},
        {QString(p::kPointerRadius), pointerRadius},
    };
}

GlassParameters GlassParameters::preset(QStringView name) {
    GlassParameters m;
    if (name == QLatin1String("crystal")) {
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

GlassRequest::GlassRequest(QObject *parent)
    : QObject(parent)
    , m_watcher(std::make_unique<QDBusServiceWatcher>(
          QString(protocol::kService),
          QDBusConnection::sessionBus(),
          QDBusServiceWatcher::WatchForOwnerChange
      )) {
    m_syncTimer.setSingleShot(true);
    m_syncTimer.setInterval(0);
    connect(&m_syncTimer, &QTimer::timeout, this, &GlassRequest::sync);

    connect(m_watcher.get(), &QDBusServiceWatcher::serviceOwnerChanged, this,
        [this](const QString &, const QString &, const QString &newOwner) {
            // The effect was (re)loaded or unloaded; any handle we held is gone either way.
            m_handle.clear();
            m_registering = false;
            setActive(false);
            setAvailable(!newOwner.isEmpty());
            scheduleSync();
        });

    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.connect(QString(protocol::kService), QString(protocol::kPath), QString(protocol::kInterface),
        QStringLiteral("Attached"), this, SLOT(onAttached(QString, QString)));
    bus.connect(QString(protocol::kService), QString(protocol::kPath), QString(protocol::kInterface),
        QStringLiteral("Detached"), this, SLOT(onDetached(QString)));

    // Asynchronous presence check so construction never blocks the GUI thread.
    if (QDBusConnectionInterface *iface = bus.interface()) {
        auto *watcher = new QDBusPendingCallWatcher(
            iface->asyncCall(QStringLiteral("NameHasOwner"), QString(protocol::kService)), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *call) {
            const QDBusPendingReply<bool> reply = *call;
            setAvailable(reply.isValid() && reply.value());
            scheduleSync();
            call->deleteLater();
        });
    }
}

GlassRequest::~GlassRequest() {
    unregisterNow();
    if (m_window) {
        blurOwners()[m_window.data()].removeAll(this);
        if (m_blurApplied) {
            m_blurApplied = false;
            m_region = {};
            applyBlurRegion();
        }
    }
}

void GlassRequest::setWindow(QWindow *window) {
    if (m_window == window) {
        return;
    }
    disconnect(m_sizeConnection);
    disconnect(m_visibleConnection);
    unregisterNow();
    if (m_window) {
        blurOwners()[m_window.data()].removeAll(this);
        if (m_blurApplied) {
            const QRectF region = m_region;
            m_region = {};
            applyBlurRegion(); // hand the rest of the shared region back
            m_region = region;
            m_blurApplied = false;
        }
    }
    m_window = window;
    if (m_window) {
        blurOwners()[m_window.data()].append(this);
        // While unattached the effect matches by size, so keep it current. Once attached the
        // binding is stable and a resize is only a cheap no-op Set().
        m_sizeConnection = connect(m_window, &QWindow::widthChanged, this, &GlassRequest::scheduleSync);
        connect(m_window, &QWindow::heightChanged, this, &GlassRequest::scheduleSync);
        m_visibleConnection = connect(m_window, &QWindow::visibleChanged, this, &GlassRequest::scheduleSync);
    }
    scheduleSync();
}

QWindow *GlassRequest::window() const {
    return m_window;
}

void GlassRequest::setEnabled(bool enabled) {
    if (m_enabled == enabled) {
        return;
    }
    m_enabled = enabled;
    Q_EMIT enabledChanged();
    scheduleSync();
}

bool GlassRequest::isEnabled() const {
    return m_enabled;
}

void GlassRequest::setParameters(const GlassParameters &parameters) {
    if (m_params == parameters) {
        return;
    }
    m_params = parameters;
    scheduleSync();
}

const GlassParameters &GlassRequest::parameters() const {
    return m_params;
}

void GlassRequest::setExtraParameters(const QVariantMap &extra) {
    if (m_extra == extra) {
        return;
    }
    m_extra = extra;
    scheduleSync();
}

void GlassRequest::setMatchHints(const QVariantMap &hints) {
    if (m_matchHints == hints) {
        return;
    }
    m_matchHints = hints;
    scheduleSync();
}

void GlassRequest::setRegion(const QRectF &rectInWindow, qreal radius) {
    if (m_region == rectInWindow && qFuzzyCompare(m_regionRadius + 1.0, radius + 1.0)) {
        return;
    }
    m_region = rectInWindow;
    m_regionRadius = radius;
    m_params.radius = radius;
    scheduleSync();
}

void GlassRequest::setManageBlurRegion(bool manage) {
    if (m_manageBlur == manage) {
        return;
    }
    if (!manage && m_window && m_blurApplied) {
        KWindowEffects::enableBlurBehind(m_window, false);
        m_blurApplied = false;
    }
    m_manageBlur = manage;
    scheduleSync();
}

bool GlassRequest::managesBlurRegion() const {
    return m_manageBlur;
}

void GlassRequest::setBlurOnly(bool blurOnly) {
    if (m_blurOnly == blurOnly) {
        return;
    }
    m_blurOnly = blurOnly;
    scheduleSync();
}

bool GlassRequest::isBlurOnly() const {
    return m_blurOnly;
}

bool GlassRequest::isAvailable() const {
    return m_available;
}

bool GlassRequest::isActive() const {
    return m_active;
}

QString GlassRequest::windowUuid() const {
    return m_windowUuid;
}

bool GlassRequest::serviceRegistered() {
    QDBusConnectionInterface *iface = QDBusConnection::sessionBus().interface();
    return iface && iface->isServiceRegistered(QString(protocol::kService)).value();
}

QRegion GlassRequest::roundedRegion(const QRectF &rect, qreal radius) {
    const QRectF inset = rect.adjusted(1.0, 1.0, -1.0, -1.0);
    if (inset.width() <= 2.0 || inset.height() <= 2.0) {
        return {};
    }
    const qreal maxR = std::min(inset.width(), inset.height()) * 0.5;
    const qreal r = std::clamp<qreal>(radius - 1.0, 0.0, maxR);
    if (r <= 0.5) {
        return QRegion(inset.toAlignedRect());
    }
    QPainterPath path;
    path.addRoundedRect(inset, r, r);
    return QRegion(path.toFillPolygon().toPolygon());
}

void GlassRequest::scheduleSync() {
    if (!m_syncTimer.isActive()) {
        m_syncTimer.start();
    }
}

void GlassRequest::applyBlurRegion() {
    if (!m_manageBlur || !m_window) {
        return;
    }
    // The union of every managed request on this window: the frosted fallback, and with the
    // effect running the glass shapes (each request selects its own piece via `shapes`).
    QRegion region;
    const auto owners = blurOwners().value(m_window.data());
    for (const GlassRequest *request : owners) {
        if (request->m_window == m_window && request->m_manageBlur && request->m_enabled && !request->m_region.isEmpty()) {
            region += roundedRegion(request->m_region, request->m_regionRadius);
        }
    }
    if (!region.isEmpty()) {
        KWindowEffects::enableBlurBehind(m_window, true, region);
        m_blurApplied = true;
    } else if (m_blurApplied || owners.size() <= 1) {
        KWindowEffects::enableBlurBehind(m_window, false);
        m_blurApplied = false;
    }
}

QVariantMap GlassRequest::matchMap() const {
    namespace m = protocol::match;
    QVariantMap map;
    if (m_window) {
        map.insert(QString(m::kWidth), m_window->width());
        map.insert(QString(m::kHeight), m_window->height());
        if (!m_window->title().isEmpty()) {
            map.insert(QString(m::kTitle), m_window->title());
        }
    }
    const QString appId = QGuiApplication::desktopFileName();
    if (!appId.isEmpty()) {
        map.insert(QString(m::kAppId), appId);
    }
    for (auto it = m_matchHints.cbegin(); it != m_matchHints.cend(); ++it) {
        map.insert(it.key(), it.value());
    }
    return map;
}

QVariantMap GlassRequest::paramMap() const {
    GlassParameters params = m_params;
    if (m_manageBlur && !m_region.isEmpty()) {
        params.inset = 1.0; // undo roundedRegion()'s inset
    }
    QVariantMap map = params.toVariantMap();
    if (!m_region.isEmpty()) {
        // Selects our piece of the window's blur region (geometry still comes from the region,
        // which is committed with the surface, so it never lags behind a resize).
        map.insert(QString(protocol::param::kShapes), QVariantList{
            m_region.x(), m_region.y(), m_region.width(), m_region.height(), m_regionRadius,
        });
    }
    for (auto it = m_extra.cbegin(); it != m_extra.cend(); ++it) {
        map.insert(it.key(), it.value());
    }
    return map;
}

void GlassRequest::sync() {
    applyBlurRegion();
    const bool want = m_enabled && m_window && m_available && !m_blurOnly;
    if (!want) {
        unregisterNow();
        return;
    }
    if (m_registering) {
        return; // the reply handler calls sync() again
    }
    if (m_handle.isEmpty()) {
        registerNow();
        return;
    }
    QDBusMessage message = glassCall(QStringLiteral("Set"));
    message << m_handle << matchMap() << paramMap();
    // Set() answers with the bound window's UUID (empty while unattached), so the attach state
    // never depends on catching the Attached signal.
    const QString handle = m_handle;
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, handle](QDBusPendingCallWatcher *call) {
        const QDBusPendingReply<QString> reply = *call;
        call->deleteLater();
        if (handle != m_handle) {
            return;
        }
        if (reply.isError()) {
            // Unknown handle (e.g. effect reloaded between calls): register afresh.
            m_handle.clear();
            setActive(false);
            scheduleSync();
            return;
        }
        setActive(!reply.value().isEmpty(), reply.value());
    });
}

void GlassRequest::registerNow() {
    QDBusMessage message = glassCall(QStringLiteral("Register"));
    message << matchMap() << paramMap();
    m_registering = true;
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *call) {
        const QDBusPendingReply<QString> reply = *call;
        call->deleteLater();
        if (!m_registering) {
            return; // service owner changed meanwhile; the new owner knows nothing of us
        }
        m_registering = false;
        if (reply.isValid()) {
            m_handle = reply.value();
        }
        sync(); // apply anything that changed in flight, or unregister if no longer wanted
    });
}

void GlassRequest::unregisterNow() {
    if (!m_handle.isEmpty()) {
        QDBusMessage message = glassCall(QStringLiteral("Unregister"));
        message << m_handle;
        QDBusConnection::sessionBus().asyncCall(message);
        m_handle.clear();
    }
    setActive(false);
}

void GlassRequest::onAttached(const QString &handle, const QString &windowUuid) {
    if (!m_handle.isEmpty() && handle == m_handle) {
        setActive(true, windowUuid);
    }
}

void GlassRequest::onDetached(const QString &handle) {
    if (!m_handle.isEmpty() && handle == m_handle) {
        setActive(false);
    }
}

void GlassRequest::setAvailable(bool available) {
    if (m_available == available) {
        return;
    }
    m_available = available;
    Q_EMIT availableChanged();
}

void GlassRequest::setActive(bool active, const QString &uuid) {
    m_windowUuid = active ? uuid : QString();
    if (m_active == active) {
        return;
    }
    m_active = active;
    Q_EMIT activeChanged();
}

} // namespace fluctus::glass
