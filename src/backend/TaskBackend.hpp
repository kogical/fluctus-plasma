#pragma once

#include <QString>
#include <QUrl>
#include <QVariantList>

namespace fluctus::backend {

/// Desktop-entry integration helpers (Jump List / KService actions).
class TaskBackend {
public:
    /// Returns Jump List actions (`[{name, text, iconName}]`) from the task's `.desktop` file.
    [[nodiscard]] static QVariantList jumpListActions(const QUrl &launcherUrl);

    /// Triggers a named `KServiceAction` on the task's `.desktop` file.
    static bool triggerJumpListAction(const QUrl &launcherUrl, const QString &actionName);
};

} // namespace fluctus::backend
