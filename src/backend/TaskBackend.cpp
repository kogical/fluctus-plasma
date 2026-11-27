#include "backend/TaskBackend.hpp"

#include <KIO/ApplicationLauncherJob>
#include <KService>
#include <taskmanager/tasktools.h>

#include <QVariantMap>

namespace fluctus::backend {

namespace {

[[nodiscard]] KService::Ptr resolveService(const QUrl &launcherUrl) {
    if (!launcherUrl.isValid() || launcherUrl.isEmpty()) {
        return {};
    }

    const TaskManager::AppData appData = TaskManager::appDataFromUrl(launcherUrl);
    if (!appData.id.isEmpty()) {
        if (KService::Ptr svc = KService::serviceByStorageId(appData.id)) {
            return svc;
        }
    }

    const QUrl resolvedUrl = appData.url.isValid() ? appData.url : launcherUrl;
    if (resolvedUrl.isLocalFile()) {
        if (KService::Ptr svc = KService::serviceByDesktopPath(resolvedUrl.toLocalFile())) {
            return svc;
        }
    }

    if (resolvedUrl.scheme() == QLatin1String("applications")) {
        const QString storageId = resolvedUrl.path();
        if (KService::Ptr svc = KService::serviceByStorageId(storageId)) {
            return svc;
        }
    }

    return {};
}

} // namespace

QVariantList TaskBackend::jumpListActions(const QUrl &launcherUrl) {
    QVariantList result;
    const KService::Ptr service = resolveService(launcherUrl);
    if (!service) {
        return result;
    }

    const QList<KServiceAction> actions = service->actions();
    result.reserve(actions.size());
    for (const KServiceAction &action : actions) {
        if (action.isSeparator() || action.noDisplay() || action.text().isEmpty()) {
            continue;
        }
        result.push_back(QVariantMap{
            {QStringLiteral("name"), action.name()},
            {QStringLiteral("text"), action.text()},
            {QStringLiteral("iconName"), action.icon()},
        });
    }
    return result;
}

bool TaskBackend::triggerJumpListAction(const QUrl &launcherUrl, const QString &actionName) {
    if (actionName.isEmpty()) {
        return false;
    }

    const KService::Ptr service = resolveService(launcherUrl);
    if (!service) {
        return false;
    }

    const QList<KServiceAction> actions = service->actions();
    for (const KServiceAction &action : actions) {
        if (action.name() == actionName) {
            auto *job = new KIO::ApplicationLauncherJob(action);
            job->start();
            return true;
        }
    }
    return false;
}

} // namespace fluctus::backend
