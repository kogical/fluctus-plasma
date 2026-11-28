// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0

#include "backend/ScintillaBridge.hpp"

#include <scintilla.h>

#include <QMetaObject>

#include <memory>

namespace fluctus::battery::backend {

namespace {

struct ScintillaClientDeleter {
    void operator()(ScintillaClient *ptr) const noexcept {
        scintilla_client_free(ptr);
    }
};

struct ScintillaStringDeleter {
    void operator()(char *ptr) const noexcept {
        scintilla_string_free(ptr);
    }
};

using ScintillaClientPtr = std::unique_ptr<ScintillaClient, ScintillaClientDeleter>;
using ScintillaStringPtr = std::unique_ptr<char, ScintillaStringDeleter>;

} // namespace

ScintillaBridge::ScintillaBridge(QObject *parent)
    : QObject(parent)
    , m_worker([this](const std::stop_token &token) { workerLoop(token); }) {}

ScintillaBridge::~ScintillaBridge() {
    m_worker.request_stop();
    m_cv.notify_all();
}

void ScintillaBridge::requestDiscovery() {
    {
        std::scoped_lock lock(m_mutex);
        m_pending = true;
    }
    m_cv.notify_one();
}

void ScintillaBridge::workerLoop(const std::stop_token &stopToken) {
    // Always include stubs at the provider level; BatteryDeviceModel filters them cleanly
    // in memory so toggling "Show connected stubs" in settings takes effect instantaneously.
    ScintillaClientPtr client(scintilla_client_new(true, true));
    if (!client) {
        return;
    }

    while (!stopToken.stop_requested()) {
        {
            std::unique_lock lock(m_mutex);
            m_cv.wait(lock, stopToken, [this]() { return m_pending; });
            if (stopToken.stop_requested()) {
                break;
            }
            m_pending = false;
        }

        QMetaObject::invokeMethod(
            this,
            [this]() { Q_EMIT discoveryStarted(); },
            Qt::QueuedConnection
        );

        ScintillaStringPtr rawJson(scintilla_client_discover_json(client.get()));
        QByteArray snapshot;
        if (rawJson) {
            snapshot = QByteArray(rawJson.get());
        }

        if (stopToken.stop_requested()) {
            break;
        }

        QMetaObject::invokeMethod(
            this,
            [this, snapshot = std::move(snapshot)]() { Q_EMIT discoveryCompleted(snapshot); },
            Qt::QueuedConnection
        );
    }
}

} // namespace fluctus::battery::backend
