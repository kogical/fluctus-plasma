// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QByteArray>
#include <QObject>

#include <condition_variable>
#include <mutex>
#include <thread>

namespace fluctus::battery::backend {

/// Asynchronous worker bridging the Rust `scintilla` C ABI onto a dedicated background thread
/// so discovery and peripheral probing never block the Plasma GUI thread.
class ScintillaBridge final : public QObject {
    Q_OBJECT

public:
    explicit ScintillaBridge(QObject *parent = nullptr);
    ~ScintillaBridge() override;

    ScintillaBridge(const ScintillaBridge &) = delete;
    ScintillaBridge &operator=(const ScintillaBridge &) = delete;

    /// Queues an asynchronous discovery pass. Multiple rapid calls coalesce into one pass.
    void requestDiscovery();

Q_SIGNALS:
    void discoveryStarted();
    void discoveryCompleted(const QByteArray &jsonSnapshot);

private:
    void workerLoop(const std::stop_token &stopToken);

    std::mutex m_mutex;
    std::condition_variable_any m_cv;
    bool m_pending{false};
    std::jthread m_worker;
};

} // namespace fluctus::battery::backend
