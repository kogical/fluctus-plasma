// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: GPL-3.0-or-later
//
// com.kogical.Fluctus.Glass1 D-Bus service exported from inside KWin. QtCore/QtDBus only.
// Never blocks: the caller's pid is fetched asynchronously from the bus daemon, and every
// handle may only be used by the connection that created it.
#pragma once

#include "material.hpp"

#include <QDBusContext>
#include <QObject>
#include <QString>
#include <QUuid>
#include <QVariantMap>

#include <functional>
#include <map>
#include <memory>
#include <optional>

class QDBusServiceWatcher;

namespace fluctus::glass {

struct Registration {
    QString handle;
    QString owner;              ///< unique bus name of the client
    std::optional<qint64> pid;  ///< known once the bus daemon answered
    QVariantMap match;
    Material material;
    QUuid window;               ///< bound EffectWindow, null while unattached
};

class GlassService : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.kogical.Fluctus.Glass1")

public:
    /// Called whenever a registration was added, changed (incl. pid resolved) or removed.
    using ChangeListener = std::function<void(const QString &handle)>;

    explicit GlassService(QObject *parent = nullptr);
    ~GlassService() override;

    GlassService(const GlassService &) = delete;
    GlassService &operator=(const GlassService &) = delete;

    /// Exports the object and claims the well-known name. Returns false if either failed.
    bool start();
    void stop();

    void setChangeListener(ChangeListener listener);
    void setGlobalMode(bool global);

    [[nodiscard]] const std::map<QString, Registration> &registrations() const;
    [[nodiscard]] Registration *find(const QString &handle);

    /// Records the binding and emits Attached / Detached.
    void bind(const QString &handle, const QUuid &window);
    void unbind(const QString &handle);
    /// Detaches every registration bound to `window` (window closed).
    void unbindWindow(const QUuid &window);

    /// Test hook: create a registration as if `owner` had called Register().
    QString registerFor(const QString &owner, const QVariantMap &match, const QVariantMap &params);
    void setPidForTesting(const QString &handle, qint64 pid);

public Q_SLOTS:
    QString Register(const QVariantMap &match, const QVariantMap &params);
    QString Set(const QString &handle, const QVariantMap &match, const QVariantMap &params);
    void Unregister(const QString &handle);
    QVariantMap GetInfo();

Q_SIGNALS:
    void Attached(const QString &handle, const QString &windowUuid);
    void Detached(const QString &handle);

private:
    void lookupPid(const QString &owner);
    void dropOwner(const QString &owner);
    void notify(const QString &handle);
    [[nodiscard]] QString caller() const;
    [[nodiscard]] Registration *owned(const QString &handle);

    std::map<QString, Registration> m_registrations;
    std::map<QString, qint64> m_ownerPids;
    std::unique_ptr<QDBusServiceWatcher> m_ownerWatcher;
    ChangeListener m_listener;
    quint64 m_nextHandle{1};
    bool m_globalMode{false};
    bool m_exported{false};
    bool m_serviceRegistered{false};
};

} // namespace fluctus::glass
