#include "backend/SmartLauncherBackend.hpp"

#include <QDBusConnection>
#include <QUrl>

#include <algorithm>

namespace fluctus::backend {

SmartLauncherBackend &SmartLauncherBackend::instance() {
    static SmartLauncherBackend singleton;
    return singleton;
}

SmartLauncherBackend::SmartLauncherBackend(QObject *parent)
    : QObject(parent) {
    QDBusConnection::sessionBus().connect(
        QString(),
        QString(),
        QStringLiteral("com.canonical.Unity.LauncherEntry"),
        QStringLiteral("Update"),
        this,
        SLOT(onLauncherEntryUpdate(QString,QVariantMap))
    );
}

QString SmartLauncherBackend::normalizeKey(const QString &raw) noexcept {
    QString key = raw.trimmed();
    if (key.isEmpty()) {
        return {};
    }

    if (key.startsWith(QLatin1String("application://"))) {
        key.remove(0, 14);
    } else if (key.startsWith(QLatin1String("applications:"))) {
        key.remove(0, 13);
    } else if (key.startsWith(QLatin1String("file://"))) {
        const QUrl url(key);
        key = url.fileName();
    }

    const qsizetype queryIdx = key.indexOf(QLatin1Char('?'));
    if (queryIdx >= 0) {
        key.truncate(queryIdx);
    }

    const qsizetype slashIdx = key.lastIndexOf(QLatin1Char('/'));
    if (slashIdx >= 0) {
        key = key.mid(slashIdx + 1);
    }

    if (key.endsWith(QLatin1String(".desktop"), Qt::CaseInsensitive)) {
        key.chop(8);
    }

    return key.toLower();
}

void SmartLauncherBackend::onLauncherEntryUpdate(const QString &uri, const QVariantMap &properties) {
    const QString key = normalizeKey(uri);
    if (key.isEmpty()) {
        return;
    }

    Entry entry = m_entries.value(key);

    if (properties.contains(QStringLiteral("count"))) {
        entry.count = properties.value(QStringLiteral("count")).toInt();
    }
    if (properties.contains(QStringLiteral("count-visible"))) {
        entry.countVisible = properties.value(QStringLiteral("count-visible")).toBool();
    }
    if (properties.contains(QStringLiteral("progress"))) {
        entry.progress = std::clamp(properties.value(QStringLiteral("progress")).toDouble(), 0.0, 1.0);
    }
    if (properties.contains(QStringLiteral("progress-visible"))) {
        entry.progressVisible = properties.value(QStringLiteral("progress-visible")).toBool();
    }
    if (properties.contains(QStringLiteral("urgent"))) {
        entry.urgent = properties.value(QStringLiteral("urgent")).toBool();
    }

    m_entries.insert(key, entry);
    Q_EMIT entryChanged(key);
}

QVariantMap SmartLauncherBackend::stateForApp(const QString &appId, const QString &launcherUrlStr) const {
    const QString keyA = normalizeKey(appId);
    const QString keyB = normalizeKey(launcherUrlStr);

    auto it = !keyA.isEmpty() ? m_entries.constFind(keyA) : m_entries.constEnd();
    if (it == m_entries.constEnd() && !keyB.isEmpty()) {
        it = m_entries.constFind(keyB);
    }

    if (it == m_entries.constEnd()) {
        return {};
    }

    const Entry &e = it.value();
    return QVariantMap{
        {QStringLiteral("count"), e.count},
        {QStringLiteral("countVisible"), e.countVisible},
        {QStringLiteral("progress"), e.progress},
        {QStringLiteral("progressVisible"), e.progressVisible},
        {QStringLiteral("urgent"), e.urgent},
    };
}

} // namespace fluctus::backend
