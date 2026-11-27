#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QVariantMap>

namespace fluctus::backend {

/// Process-wide singleton listening to Unity/SmartLauncher D-Bus updates
/// (`com.canonical.Unity.LauncherEntry` / `Update(QString, QVariantMap)`).
/// Shared across all Fluctus dock instances on multi-monitor setups.
class SmartLauncherBackend final : public QObject {
    Q_OBJECT

public:
    struct Entry {
        int count{0};
        bool countVisible{false};
        double progress{0.0};
        bool progressVisible{false};
        bool urgent{false};
    };

    [[nodiscard]] static SmartLauncherBackend &instance();

    [[nodiscard]] QVariantMap stateForApp(const QString &appId, const QString &launcherUrlStr) const;
    [[nodiscard]] static QString normalizeKey(const QString &raw) noexcept;

Q_SIGNALS:
    void entryChanged(const QString &normalizedAppId);

private Q_SLOTS:
    void onLauncherEntryUpdate(const QString &uri, const QVariantMap &properties);

private:
    explicit SmartLauncherBackend(QObject *parent = nullptr);
    ~SmartLauncherBackend() override = default;

    QHash<QString, Entry> m_entries;
};

} // namespace fluctus::backend
