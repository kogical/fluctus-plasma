// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: GPL-3.0-or-later

#include "glassservice.hpp"

#include "FluctusGlass/GlassProtocol.hpp"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusError>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QLoggingCategory>

#include <algorithm>

Q_LOGGING_CATEGORY(FLUCTUS_GLASS_DBUS, "fluctus.glass.dbus", QtWarningMsg)

namespace fluctus::glass {

namespace {

/// Arrays nested in a{sv} arrive as QDBusArgument; normalise them to QVariantList.
QVariantMap normalised(const QVariantMap &map) {
    QVariantMap out = map;
    const QString shapesKey(protocol::param::kShapes);
    const auto it = out.find(shapesKey);
    if (it != out.end() && it->metaType().id() == qMetaTypeId<QDBusArgument>()) {
        const auto argument = it->value<QDBusArgument>();
        QVariantList list;
        if (argument.currentSignature() == QLatin1String("ad")) {
            const auto doubles = qdbus_cast<QList<double>>(argument);
            list.reserve(doubles.size());
            for (double d : doubles) {
                list.append(d);
            }
        } else if (argument.currentSignature() == QLatin1String("av")) {
            list = qdbus_cast<QVariantList>(argument);
        }
        *it = list;
    } else if (it != out.end() && it->metaType().id() == QMetaType::fromType<QList<double>>().id()) {
        QVariantList list;
        for (double d : it->value<QList<double>>()) {
            list.append(d);
        }
        *it = list;
    }
    return out;
}

} // namespace

GlassService::GlassService(QObject *parent)
    : QObject(parent)
    , m_ownerWatcher(std::make_unique<QDBusServiceWatcher>()) {
    m_ownerWatcher->setConnection(QDBusConnection::sessionBus());
    m_ownerWatcher->setWatchMode(QDBusServiceWatcher::WatchForUnregistration);
    connect(m_ownerWatcher.get(), &QDBusServiceWatcher::serviceUnregistered, this, &GlassService::dropOwner);
}

GlassService::~GlassService() {
    stop();
}

bool GlassService::start() {
    QDBusConnection bus = QDBusConnection::sessionBus();
    m_exported = bus.registerObject(QString(protocol::kPath), QString(protocol::kInterface), this,
        QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals);
    if (!m_exported) {
        qCWarning(FLUCTUS_GLASS_DBUS) << "Failed to export" << protocol::kPath << bus.lastError().message();
        return false;
    }
    m_serviceRegistered = bus.registerService(QString(protocol::kService));
    if (!m_serviceRegistered) {
        qCWarning(FLUCTUS_GLASS_DBUS) << "Failed to claim" << protocol::kService << bus.lastError().message();
    }
    return m_serviceRegistered;
}

void GlassService::stop() {
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (m_serviceRegistered) {
        bus.unregisterService(QString(protocol::kService));
        m_serviceRegistered = false;
    }
    if (m_exported) {
        bus.unregisterObject(QString(protocol::kPath));
        m_exported = false;
    }
}

void GlassService::setChangeListener(ChangeListener listener) {
    m_listener = std::move(listener);
}

void GlassService::setGlobalMode(bool global) {
    m_globalMode = global;
}

const std::map<QString, Registration> &GlassService::registrations() const {
    return m_registrations;
}

Registration *GlassService::find(const QString &handle) {
    const auto it = m_registrations.find(handle);
    return it == m_registrations.end() ? nullptr : &it->second;
}

void GlassService::bind(const QString &handle, const QUuid &window) {
    Registration *reg = find(handle);
    if (!reg || reg->window == window) {
        return;
    }
    reg->window = window;
    if (window.isNull()) {
        Q_EMIT Detached(handle);
    } else {
        Q_EMIT Attached(handle, window.toString(QUuid::WithoutBraces));
    }
}

void GlassService::unbind(const QString &handle) {
    bind(handle, QUuid());
}

void GlassService::unbindWindow(const QUuid &window) {
    for (auto &[handle, reg] : m_registrations) {
        if (reg.window == window) {
            reg.window = QUuid();
            Q_EMIT Detached(handle);
        }
    }
}

QString GlassService::caller() const {
    return calledFromDBus() ? message().service() : QString();
}

Registration *GlassService::owned(const QString &handle) {
    Registration *reg = find(handle);
    if (!reg || reg->owner != caller()) {
        if (calledFromDBus()) {
            sendErrorReply(QDBusError::InvalidArgs, QStringLiteral("Unknown glass handle"));
        }
        return nullptr;
    }
    return reg;
}

QString GlassService::registerFor(const QString &owner, const QVariantMap &match, const QVariantMap &params) {
    const auto count = std::count_if(m_registrations.cbegin(), m_registrations.cend(),
        [&owner](const auto &entry) { return entry.second.owner == owner; });
    if (count >= protocol::kMaxRegistrationsPerClient) {
        return {};
    }
    const QString handle = QStringLiteral("g%1").arg(m_nextHandle++);
    Registration reg;
    reg.handle = handle;
    reg.owner = owner;
    reg.match = match;
    reg.material = Material::fromVariantMap(normalised(params));
    if (const auto pid = m_ownerPids.find(owner); pid != m_ownerPids.end()) {
        reg.pid = pid->second;
    }
    m_registrations.emplace(handle, std::move(reg));
    notify(handle);
    return handle;
}

void GlassService::setPidForTesting(const QString &handle, qint64 pid) {
    if (Registration *reg = find(handle)) {
        m_ownerPids[reg->owner] = pid;
        reg->pid = pid;
        notify(handle);
    }
}

QString GlassService::Register(const QVariantMap &match, const QVariantMap &params) {
    const QString owner = caller();
    if (owner.isEmpty()) {
        return {};
    }
    const QString handle = registerFor(owner, match, params);
    if (handle.isEmpty()) {
        sendErrorReply(QDBusError::LimitsExceeded, QStringLiteral("Too many glass registrations"));
        return {};
    }
    if (!m_ownerPids.contains(owner)) {
        m_ownerWatcher->addWatchedService(owner);
        lookupPid(owner);
    }
    return handle;
}

QString GlassService::Set(const QString &handle, const QVariantMap &match, const QVariantMap &params) {
    Registration *reg = owned(handle);
    if (!reg) {
        return {};
    }
    reg->match = match;
    reg->material = Material::fromVariantMap(normalised(params));
    notify(handle);
    // notify() may have bound or rebound the window.
    if (Registration *now = find(handle); now && !now->window.isNull()) {
        return now->window.toString(QUuid::WithoutBraces);
    }
    return {};
}

void GlassService::Unregister(const QString &handle) {
    if (!owned(handle)) {
        return;
    }
    m_registrations.erase(handle);
    notify(handle);
}

QVariantMap GlassService::GetInfo() {
    return {
        {QStringLiteral("version"), protocol::kVersion},
        {QStringLiteral("globalMode"), m_globalMode},
        {QStringLiteral("capabilities"), QStringList{
            QStringLiteral("refraction"),
            QStringLiteral("dispersion"),
            QStringLiteral("frost"),
            QStringLiteral("pointer-light"),
            QStringLiteral("pointer-bulge"),
            QStringLiteral("shadow"),
            QStringLiteral("shapes"),
        }},
    };
}

void GlassService::lookupPid(const QString &owner) {
    QDBusConnectionInterface *iface = QDBusConnection::sessionBus().interface();
    if (!iface) {
        return;
    }
    auto *watcher = new QDBusPendingCallWatcher(
        iface->asyncCall(QStringLiteral("GetConnectionUnixProcessID"), owner), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, owner](QDBusPendingCallWatcher *call) {
        call->deleteLater();
        const QDBusPendingReply<uint> reply = *call;
        if (!reply.isValid()) {
            qCWarning(FLUCTUS_GLASS_DBUS) << "No pid for" << owner << reply.error().message();
            return;
        }
        const qint64 pid = reply.value();
        m_ownerPids[owner] = pid;
        QStringList changed;
        for (auto &[handle, reg] : m_registrations) {
            if (reg.owner == owner) {
                reg.pid = pid;
                changed.append(handle);
            }
        }
        for (const QString &handle : std::as_const(changed)) {
            notify(handle);
        }
    });
}

void GlassService::dropOwner(const QString &owner) {
    m_ownerWatcher->removeWatchedService(owner);
    m_ownerPids.erase(owner);
    QStringList dropped;
    for (auto it = m_registrations.begin(); it != m_registrations.end();) {
        if (it->second.owner == owner) {
            dropped.append(it->first);
            it = m_registrations.erase(it);
        } else {
            ++it;
        }
    }
    for (const QString &handle : std::as_const(dropped)) {
        notify(handle);
    }
}

void GlassService::notify(const QString &handle) {
    if (m_listener) {
        m_listener(handle);
    }
}

} // namespace fluctus::glass
