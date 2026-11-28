// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0

#include "backend/BatteryApplet.hpp"

#include "core/AdaptiveLayout.hpp"

#include <QDBusConnection>

#include <algorithm>

namespace fluctus::battery::backend {

namespace {

[[nodiscard]] QString formModeName(core::FormMode mode) {
    switch (mode) {
    case core::FormMode::MicroPanel:
        return QStringLiteral("microPanel");
    case core::FormMode::CompactPanel:
        return QStringLiteral("compactPanel");
    case core::FormMode::LargePanel:
        return QStringLiteral("largePanel");
    case core::FormMode::DesktopTiny:
        return QStringLiteral("desktopTiny");
    case core::FormMode::DesktopCompact:
        return QStringLiteral("desktopCompact");
    case core::FormMode::DesktopExpanded:
        return QStringLiteral("desktopExpanded");
    }
    return QStringLiteral("desktopCompact");
}

} // namespace

BatteryApplet::BatteryApplet(QObject *parent, const KPluginMetaData &data, const QVariantList &args)
    : Plasma::Applet(parent, data, args)
    , m_model(std::make_unique<BatteryDeviceModel>())
    , m_bridge(std::make_unique<ScintillaBridge>()) {
    m_debounceTimer.setSingleShot(true);
    m_debounceTimer.setInterval(400);
    connect(&m_debounceTimer, &QTimer::timeout, this, &BatteryApplet::refresh);

    m_pollTimer.setInterval(30'000);
    connect(&m_pollTimer, &QTimer::timeout, this, &BatteryApplet::refresh);

    connect(m_bridge.get(), &ScintillaBridge::discoveryStarted, this, [this]() {
        m_model->setRefreshing(true);
    });
    connect(m_bridge.get(), &ScintillaBridge::discoveryCompleted, this, [this](const QByteArray &json) {
        m_model->setRawDevices(parseScintillaJson(json));
        m_model->setRefreshing(false);
    });
}

BatteryApplet::~BatteryApplet() = default;

void BatteryApplet::init() {
    Plasma::Applet::init();
    connectSystemSignals();
    m_pollTimer.start();
    refresh();
}

void BatteryApplet::refresh() {
    if (m_bridge) {
        m_bridge->requestDiscovery();
    }
}

void BatteryApplet::setFilterConfig(
    bool showStubs,
    bool hideLinePower,
    int sortOrder,
    const QString &preferredDeviceId,
    int lowThreshold,
    int criticalThreshold
) {
    core::FilterOptions opts;
    opts.showStubs = showStubs;
    opts.hideLinePower = hideLinePower;
    switch (sortOrder) {
    case 1:
        opts.sortOrder = core::SortOrder::LowestFirst;
        break;
    case 2:
        opts.sortOrder = core::SortOrder::Alphabetical;
        break;
    case 0:
    default:
        opts.sortOrder = core::SortOrder::PrimaryThenLowest;
        break;
    }
    opts.preferredDeviceId = preferredDeviceId.toStdString();

    const int clampedLow = std::clamp(lowThreshold, 5, 50);
    const int clampedCrit = std::clamp(criticalThreshold, 1, clampedLow);

    m_model->setFilterOptions(opts, static_cast<double>(clampedLow), static_cast<double>(clampedCrit));
}

void BatteryApplet::setPollIntervalSeconds(int seconds) {
    const int clamped = std::clamp(seconds, 10, 600);
    m_pollTimer.setInterval(clamped * 1000);
}

QVariantMap BatteryApplet::layoutMetrics(
    qreal width,
    qreal height,
    bool inPanel,
    bool verticalPanel
) const {
    const auto m = core::computeLayoutMetrics(width, height, inPanel, verticalPanel);
    return {
        {QStringLiteral("mode"), formModeName(m.mode)},
        {QStringLiteral("columns"), m.columns},
        {QStringLiteral("iconSize"), m.iconSize},
        {QStringLiteral("ringSize"), m.ringSize},
        {QStringLiteral("fontSizePx"), m.fontSizePx},
        {QStringLiteral("subFontSizePx"), m.subFontSizePx},
        {QStringLiteral("padding"), m.padding},
        {QStringLiteral("spacing"), m.spacing},
        {QStringLiteral("cornerRadius"), m.cornerRadius},
        {QStringLiteral("showSecondaryLabel"), m.showSecondaryLabel},
        {QStringLiteral("stackVertically"), m.stackVertically},
    };
}

void BatteryApplet::scheduleDebouncedRefresh() {
    m_debounceTimer.start();
}

void BatteryApplet::connectSystemSignals() {
    QDBusConnection bus = QDBusConnection::systemBus();
    if (!bus.isConnected()) {
        return;
    }

    // UPower device add/remove & property updates
    bus.connect(
        QStringLiteral("org.freedesktop.UPower"),
        QStringLiteral("/org/freedesktop/UPower"),
        QStringLiteral("org.freedesktop.UPower"),
        QStringLiteral("DeviceAdded"),
        this,
        SLOT(scheduleDebouncedRefresh())
    );
    bus.connect(
        QStringLiteral("org.freedesktop.UPower"),
        QStringLiteral("/org/freedesktop/UPower"),
        QStringLiteral("org.freedesktop.UPower"),
        QStringLiteral("DeviceRemoved"),
        this,
        SLOT(scheduleDebouncedRefresh())
    );
    bus.connect(
        QStringLiteral("org.freedesktop.UPower"),
        QString(),
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged"),
        this,
        SLOT(scheduleDebouncedRefresh())
    );

    // BlueZ device & battery property updates
    bus.connect(
        QStringLiteral("org.bluez"),
        QStringLiteral("/"),
        QStringLiteral("org.freedesktop.DBus.ObjectManager"),
        QStringLiteral("InterfacesAdded"),
        this,
        SLOT(scheduleDebouncedRefresh())
    );
    bus.connect(
        QStringLiteral("org.bluez"),
        QStringLiteral("/"),
        QStringLiteral("org.freedesktop.DBus.ObjectManager"),
        QStringLiteral("InterfacesRemoved"),
        this,
        SLOT(scheduleDebouncedRefresh())
    );
    bus.connect(
        QStringLiteral("org.bluez"),
        QString(),
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged"),
        this,
        SLOT(scheduleDebouncedRefresh())
    );
}

K_PLUGIN_CLASS_WITH_JSON(BatteryApplet, "../../package/metadata.json")

} // namespace fluctus::battery::backend

#include "BatteryApplet.moc"
