// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0

#include "FluctusGlass/GlassProfile.hpp"
#include "FluctusGlass/GlassProtocol.hpp"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <cmath>

namespace fluctus::glass {

namespace {

namespace p = protocol::param;

constexpr QLatin1StringView kPresetCustom{"custom"};

bool isKnownPreset(QStringView name) {
    return name == p::kPresetGlobal
        || name == p::kPresetLight
        || name == p::kPresetMedium
        || name == p::kPresetFull
        || name == p::kPresetLiquid
        || name == p::kPresetCrystal
        || name == p::kModePlain
        || name == p::kModeNone;
}

} // namespace

GlassProfile::GlassProfile(QObject *parent)
    : QObject(parent)
    , m_watcher(std::make_unique<QDBusServiceWatcher>(
          QString(protocol::kService),
          QDBusConnection::sessionBus(),
          QDBusServiceWatcher::WatchForOwnerChange
      )) {
    connect(m_watcher.get(), &QDBusServiceWatcher::serviceOwnerChanged, this,
        [this](const QString &, const QString &, const QString &newOwner) {
            const bool present = !newOwner.isEmpty();
            setAvailable(present);
            if (present) {
                fetchDefaults();
            }
        });

    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.connect(QString(protocol::kService), QString(protocol::kPath), QString(protocol::kInterface),
        QStringLiteral("DefaultsChanged"), this, SLOT(onDefaultsChanged(QVariantMap)));

    if (QDBusConnectionInterface *iface = bus.interface()) {
        auto *watcher = new QDBusPendingCallWatcher(
            iface->asyncCall(QStringLiteral("NameHasOwner"), QString(protocol::kService)), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *call) {
            const QDBusPendingReply<bool> reply = *call;
            const bool present = reply.isValid() && reply.value();
            setAvailable(present);
            if (present) {
                fetchDefaults();
            }
            call->deleteLater();
        });
    }
}

GlassProfile::~GlassProfile() = default;

bool GlassProfile::followsGlobal() const {
    return m_preset == p::kPresetGlobal;
}

const GlassParameters &GlassProfile::parameters() const {
    return followsGlobal() ? m_globalDefaults : m_params;
}

void GlassProfile::setParameters(const GlassParameters &parameters) {
    const bool wasGlobal = followsGlobal();
    const bool presetChanging = m_preset != kPresetCustom;
    if (!wasGlobal && m_params == parameters && !presetChanging) {
        return;
    }
    m_preset = QString(kPresetCustom);
    m_params = parameters;
    if (m_params.pointerBulge > 0.001) {
        m_savedPointerBulge = m_params.pointerBulge;
    }
    if (presetChanging) {
        Q_EMIT presetChanged();
    }
    Q_EMIT parametersChanged();
    Q_EMIT jsonChanged();
}

void GlassProfile::setGlobalDefaultsForTesting(const GlassParameters &defaults) {
    applyGlobalDefaults(defaults);
}

QVariantMap GlassProfile::toVariantMap() const {
    if (m_preset != kPresetCustom) {
        return {{QString(p::kPreset), m_preset}};
    }
    QVariantMap map = m_params.toVariantMap();
    map.insert(QString(p::kPreset), QString(kPresetCustom));
    return map;
}

void GlassProfile::loadVariantMap(const QVariantMap &map) {
    const QString presetName = map.value(QString(p::kPreset)).toString();
    if (presetName.isEmpty() || presetName == p::kPresetGlobal) {
        resetToGlobal();
        return;
    }
    if (presetName == kPresetCustom) {
        GlassParameters parsed = GlassParameters::fromVariantMap(map, m_globalDefaults);
        setParameters(parsed);
        return;
    }
    if (isKnownPreset(presetName)) {
        applyPreset(presetName);
        return;
    }
    // Map without a recognised preset name: treat as custom overrides on top of global defaults.
    setParameters(GlassParameters::fromVariantMap(map, m_globalDefaults));
}

QString GlassProfile::toJson() const {
    const QJsonObject obj = QJsonObject::fromVariantMap(toVariantMap());
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

bool GlassProfile::loadJson(const QString &json) {
    const QString trimmed = json.trimmed();
    if (trimmed.isEmpty()) {
        resetToGlobal();
        return true;
    }
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(trimmed.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }
    loadVariantMap(doc.object().toVariantMap());
    return true;
}

void GlassProfile::applyPreset(const QString &name) {
    const QString target = isKnownPreset(name) ? name : QString(p::kPresetGlobal);
    if (target == p::kPresetGlobal) {
        resetToGlobal();
        return;
    }
    const GlassParameters next = GlassParameters::preset(target);
    const bool presetChanging = m_preset != target;
    const bool paramsChanging = followsGlobal() || m_params != next;
    if (!presetChanging && !paramsChanging) {
        return;
    }
    m_preset = target;
    m_params = next;
    if (m_params.pointerBulge > 0.001) {
        m_savedPointerBulge = m_params.pointerBulge;
    }
    if (presetChanging) {
        Q_EMIT presetChanged();
    }
    if (paramsChanging) {
        Q_EMIT parametersChanged();
    }
    Q_EMIT jsonChanged();
}

void GlassProfile::resetToGlobal() {
    const bool presetChanging = m_preset != p::kPresetGlobal;
    const bool paramsChanging = m_params != m_globalDefaults;
    if (!presetChanging && !paramsChanging) {
        return;
    }
    m_preset = QString(p::kPresetGlobal);
    m_params = m_globalDefaults;
    if (presetChanging) {
        Q_EMIT presetChanged();
    }
    if (paramsChanging || presetChanging) {
        Q_EMIT parametersChanged();
    }
    Q_EMIT jsonChanged();
}

QStringList GlassProfile::presetNames() {
    return {
        QString(p::kPresetGlobal),
        QString(p::kPresetLight),
        QString(p::kPresetMedium),
        QString(p::kPresetFull),
        QString(p::kPresetCrystal),
        QString(p::kModePlain),
        QString(p::kModeNone),
    };
}

void GlassProfile::beginCustomEdit() {
    if (followsGlobal()) {
        m_params = m_globalDefaults;
    }
}

void GlassProfile::setRealField(qreal GlassParameters::*field, qreal value, qreal lo, qreal hi) {
    if (!std::isfinite(value)) {
        return;
    }
    const qreal clamped = std::clamp(value, lo, hi);
    if (!followsGlobal() && m_preset == kPresetCustom && qFuzzyCompare(m_params.*field + 1.0, clamped + 1.0)) {
        return;
    }
    beginCustomEdit();
    const bool presetChanging = m_preset != kPresetCustom;
    m_preset = QString(kPresetCustom);
    m_params.*field = clamped;
    if (presetChanging) {
        Q_EMIT presetChanged();
    }
    Q_EMIT parametersChanged();
    Q_EMIT jsonChanged();
}

void GlassProfile::setMode(const QString &value) {
    if (value != p::kModeGlass && value != p::kModePlain && value != p::kModeNone) {
        return;
    }
    if (!followsGlobal() && m_preset == kPresetCustom && m_params.mode == value) {
        return;
    }
    beginCustomEdit();
    const bool presetChanging = m_preset != kPresetCustom;
    m_preset = QString(kPresetCustom);
    m_params.mode = value;
    if (presetChanging) {
        Q_EMIT presetChanged();
    }
    Q_EMIT parametersChanged();
    Q_EMIT jsonChanged();
}

void GlassProfile::setIntensity(qreal value) { setRealField(&GlassParameters::intensity, value, 0.0, 2.0); }
void GlassProfile::setRefraction(qreal value) { setRealField(&GlassParameters::refraction, value, 0.0, 128.0); }
void GlassProfile::setBezel(qreal value) { setRealField(&GlassParameters::bezel, value, 0.0, 128.0); }
void GlassProfile::setMagnify(qreal value) { setRealField(&GlassParameters::magnify, value, 0.0, 0.30); }
void GlassProfile::setDispersion(qreal value) { setRealField(&GlassParameters::dispersion, value, 0.0, 1.0); }
void GlassProfile::setFrost(qreal value) { setRealField(&GlassParameters::frost, value, 0.0, 1.0); }
void GlassProfile::setSaturation(qreal value) { setRealField(&GlassParameters::saturation, value, 0.0, 3.0); }
void GlassProfile::setBrightness(qreal value) { setRealField(&GlassParameters::brightness, value, 0.5, 2.0); }
void GlassProfile::setHighlight(qreal value) { setRealField(&GlassParameters::highlight, value, 0.0, 2.0); }
void GlassProfile::setEdgeShade(qreal value) { setRealField(&GlassParameters::edgeShade, value, 0.0, 1.0); }
void GlassProfile::setSheen(qreal value) { setRealField(&GlassParameters::sheen, value, 0.0, 1.0); }

QColor GlassProfile::tintColor() const {
    QColor c = parameters().tint;
    c.setAlphaF(1.0F);
    return c;
}

void GlassProfile::setTintColor(const QColor &color) {
    if (!color.isValid()) {
        return;
    }
    QColor next = color;
    next.setAlphaF(parameters().tint.alphaF());
    if (!followsGlobal() && m_preset == kPresetCustom && m_params.tint == next) {
        return;
    }
    beginCustomEdit();
    const bool presetChanging = m_preset != kPresetCustom;
    m_preset = QString(kPresetCustom);
    m_params.tint = next;
    if (presetChanging) {
        Q_EMIT presetChanged();
    }
    Q_EMIT parametersChanged();
    Q_EMIT jsonChanged();
}

void GlassProfile::setTintStrength(qreal value) {
    if (!std::isfinite(value)) {
        return;
    }
    const qreal clamped = std::clamp(value, 0.0, 1.0);
    QColor next = parameters().tint;
    next.setAlphaF(static_cast<float>(clamped));
    if (!followsGlobal() && m_preset == kPresetCustom && m_params.tint == next) {
        return;
    }
    beginCustomEdit();
    const bool presetChanging = m_preset != kPresetCustom;
    m_preset = QString(kPresetCustom);
    m_params.tint = next;
    if (presetChanging) {
        Q_EMIT presetChanged();
    }
    Q_EMIT parametersChanged();
    Q_EMIT jsonChanged();
}

bool GlassProfile::pointerEffects() const {
    const GlassParameters &p = parameters();
    return p.pointerLight > 0.001 || p.pointerBulge > 0.001;
}

void GlassProfile::setPointerEffects(bool enabled) {
    if (!followsGlobal() && m_preset == kPresetCustom && pointerEffects() == enabled) {
        return;
    }
    beginCustomEdit();
    const bool presetChanging = m_preset != kPresetCustom;
    m_preset = QString(kPresetCustom);
    if (enabled) {
        m_params.pointerLight = 0.6;
        m_params.pointerBulge = m_savedPointerBulge > 0.001 ? m_savedPointerBulge : 6.0;
    } else {
        if (m_params.pointerBulge > 0.001) {
            m_savedPointerBulge = m_params.pointerBulge;
        }
        m_params.pointerLight = 0.0;
        m_params.pointerBulge = 0.0;
    }
    if (presetChanging) {
        Q_EMIT presetChanged();
    }
    Q_EMIT parametersChanged();
    Q_EMIT jsonChanged();
}

void GlassProfile::setPointerBulge(qreal value) {
    if (!std::isfinite(value)) {
        return;
    }
    const qreal clamped = std::clamp(value, 0.0, 32.0);
    if (clamped > 0.001) {
        m_savedPointerBulge = clamped;
    }
    setRealField(&GlassParameters::pointerBulge, clamped, 0.0, 32.0);
}

void GlassProfile::onDefaultsChanged(const QVariantMap &defaults) {
    applyGlobalDefaults(GlassParameters::fromVariantMap(defaults, GlassParameters::globalFallback()));
}

void GlassProfile::fetchDefaults() {
    QDBusMessage message = QDBusMessage::createMethodCall(
        QString(protocol::kService),
        QString(protocol::kPath),
        QString(protocol::kInterface),
        QStringLiteral("GetDefaults"));
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *call) {
        const QDBusPendingReply<QVariantMap> reply = *call;
        call->deleteLater();
        if (reply.isValid()) {
            applyGlobalDefaults(GlassParameters::fromVariantMap(reply.value(), GlassParameters::globalFallback()));
        }
    });
}

void GlassProfile::setAvailable(bool available) {
    if (m_available == available) {
        return;
    }
    m_available = available;
    Q_EMIT availableChanged();
}

void GlassProfile::applyGlobalDefaults(const GlassParameters &defaults) {
    if (m_globalDefaults == defaults) {
        return;
    }
    m_globalDefaults = defaults;
    Q_EMIT globalDefaultsChanged();
    if (followsGlobal()) {
        m_params = m_globalDefaults;
        Q_EMIT parametersChanged();
    }
}

} // namespace fluctus::glass
