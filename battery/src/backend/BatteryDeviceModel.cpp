// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0

#include "backend/BatteryDeviceModel.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariantMap>

#include <utility>

namespace fluctus::battery::backend {

namespace {

[[nodiscard]] QString severityString(core::BatterySeverity severity) {
    switch (severity) {
    case core::BatterySeverity::Critical:
        return QStringLiteral("critical");
    case core::BatterySeverity::Warning:
        return QStringLiteral("warning");
    case core::BatterySeverity::Charging:
        return QStringLiteral("charging");
    case core::BatterySeverity::Full:
        return QStringLiteral("full");
    case core::BatterySeverity::Normal:
        return QStringLiteral("normal");
    case core::BatterySeverity::Unknown:
    default:
        return QStringLiteral("unknown");
    }
}

[[nodiscard]] QString transportLabel(core::Transport transport) {
    switch (transport) {
    case core::Transport::Internal:
        return QStringLiteral("Internal");
    case core::Transport::Usb:
        return QStringLiteral("USB");
    case core::Transport::WirelessDongle:
        return QStringLiteral("Wireless");
    case core::Transport::BluetoothClassic:
        return QStringLiteral("Bluetooth");
    case core::Transport::Ble:
        return QStringLiteral("BLE");
    case core::Transport::Unknown:
    default:
        return {};
    }
}

[[nodiscard]] QString chargingLabel(core::ChargingState state) {
    switch (state) {
    case core::ChargingState::Charging:
        return QStringLiteral("Charging");
    case core::ChargingState::Discharging:
        return QStringLiteral("Discharging");
    case core::ChargingState::Full:
        return QStringLiteral("Fully charged");
    case core::ChargingState::NotCharging:
        return QStringLiteral("Plugged in");
    case core::ChargingState::Unknown:
    default:
        return {};
    }
}

[[nodiscard]] QString cellRoleLabel(core::CellRole role) {
    switch (role) {
    case core::CellRole::Left:
        return QStringLiteral("L");
    case core::CellRole::Right:
        return QStringLiteral("R");
    case core::CellRole::Case:
        return QStringLiteral("Case");
    case core::CellRole::Dock:
        return QStringLiteral("Dock");
    case core::CellRole::Spare:
        return QStringLiteral("Spare");
    case core::CellRole::Primary:
    default:
        return QStringLiteral("Main");
    }
}

[[nodiscard]] QString unavailableLabel(core::UnavailableReason reason) {
    switch (reason) {
    case core::UnavailableReason::NoBattery:
        return QStringLiteral("Connected");
    case core::UnavailableReason::UnsupportedProtocol:
        return QStringLiteral("No battery telemetry");
    case core::UnavailableReason::PermissionDenied:
        return QStringLiteral("Permission needed (udev)");
    case core::UnavailableReason::StackFeatureDisabled:
        return QStringLiteral("Bluetooth experimental off");
    case core::UnavailableReason::DeviceAsleep:
        return QStringLiteral("Asleep");
    case core::UnavailableReason::ReadError:
        return QStringLiteral("Read error");
    case core::UnavailableReason::None:
    default:
        return {};
    }
}

[[nodiscard]] QVariantList cellsToVariantList(const core::BatteryDevice &device) {
    QVariantList list;
    list.reserve(static_cast<qsizetype>(device.cells.size()));
    for (const auto &cell : device.cells) {
        QVariantMap map;
        map.insert(QStringLiteral("roleLabel"), cellRoleLabel(cell.role));
        map.insert(QStringLiteral("hasPercentage"), cell.percentage.has_value());
        map.insert(QStringLiteral("percentage"), cell.percentage.value_or(-1.0));
        map.insert(
            QStringLiteral("percentageText"),
            QString::fromStdString(core::formatPercentage(cell.percentage, cell.coarse))
        );
        map.insert(QStringLiteral("charging"), cell.charging == core::ChargingState::Charging);
        map.insert(QStringLiteral("voltageMv"), cell.voltageMv.has_value() ? static_cast<int>(*cell.voltageMv) : 0);
        list.push_back(std::move(map));
    }
    return list;
}

} // namespace

std::vector<core::BatteryDevice> parseScintillaJson(const QByteArray &json) {
    std::vector<core::BatteryDevice> devices;
    if (json.isEmpty()) {
        return devices;
    }

    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray()) {
        return devices;
    }

    const QJsonArray arr = doc.array();
    devices.reserve(static_cast<std::size_t>(arr.size()));

    for (const auto &val : arr) {
        if (!val.isObject()) {
            continue;
        }
        const QJsonObject obj = val.toObject();
        core::BatteryDevice dev;
        dev.id = obj.value(QStringLiteral("id")).toString().toStdString();
        dev.name = obj.value(QStringLiteral("name")).toString().toStdString();
        if (dev.name.empty()) {
            dev.name = dev.id;
        }
        if (const QJsonValue v = obj.value(QStringLiteral("vendor")); v.isString()) {
            dev.vendor = v.toString().toStdString();
        }
        if (const QJsonValue m = obj.value(QStringLiteral("model")); m.isString()) {
            dev.model = m.toString().toStdString();
        }
        dev.category = core::parseCategory(obj.value(QStringLiteral("category")).toString().toStdString());
        dev.transport = core::parseTransport(obj.value(QStringLiteral("transport")).toString().toStdString());
        dev.powerAvailable = obj.value(QStringLiteral("available")).toBool(false);
        if (const QJsonValue s = obj.value(QStringLiteral("source")); s.isString()) {
            dev.source = s.toString().toStdString();
        }
        if (const QJsonValue r = obj.value(QStringLiteral("unavailable_reason")); r.isString()) {
            dev.unavailableReason = core::parseUnavailableReason(r.toString().toStdString());
        }

        const QJsonArray cellsArr = obj.value(QStringLiteral("cells")).toArray();
        dev.cells.reserve(static_cast<std::size_t>(cellsArr.size()));
        for (const auto &cval : cellsArr) {
            if (!cval.isObject()) {
                continue;
            }
            const QJsonObject cobj = cval.toObject();
            core::BatteryCell cell;
            cell.role = core::parseCellRole(cobj.value(QStringLiteral("role")).toString().toStdString());
            if (const QJsonValue p = cobj.value(QStringLiteral("percentage")); p.isDouble()) {
                cell.percentage = p.toDouble();
            }
            if (const QJsonValue c = cobj.value(QStringLiteral("coarse")); c.isString()) {
                cell.coarse = core::parseCoarseLevel(c.toString().toStdString());
            }
            cell.charging = core::parseChargingState(cobj.value(QStringLiteral("charging")).toString().toStdString());
            if (const QJsonValue mv = cobj.value(QStringLiteral("voltage_mv")); mv.isDouble()) {
                const double rawMv = mv.toDouble();
                if (rawMv >= 0.0) {
                    cell.voltageMv = static_cast<std::uint32_t>(rawMv);
                }
            }
            dev.cells.push_back(cell);
        }

        devices.push_back(std::move(dev));
    }

    return devices;
}

BatteryDeviceModel::BatteryDeviceModel(QObject *parent)
    : QAbstractListModel(parent) {}

int BatteryDeviceModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_filtered.size());
}

QVariant BatteryDeviceModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid()) {
        return {};
    }
    const auto row = static_cast<std::size_t>(index.row());
    if (row >= m_filtered.size()) {
        return {};
    }

    const core::BatteryDevice &dev = m_filtered.at(row);
    const auto pct = core::primaryPercentage(dev);
    const auto coarse = core::primaryCoarseLevel(dev);
    const auto charging = core::aggregateChargingState(dev);
    const auto severity = core::classifySeverity(pct, charging, m_lowThreshold, m_criticalThreshold);

    switch (role) {
    case DeviceIdRole:
        return QString::fromStdString(dev.id);
    case Qt::DisplayRole:
    case NameRole:
        return QString::fromStdString(dev.name);
    case VendorRole:
        return dev.vendor.has_value() ? QString::fromStdString(*dev.vendor) : QString();
    case ModelRole:
        return dev.model.has_value() ? QString::fromStdString(*dev.model) : QString();
    case CategoryRole:
        return static_cast<int>(dev.category);
    case CategoryIconRole: {
        const auto sv = core::categoryIconName(dev.category);
        return QString::fromUtf8(sv.data(), static_cast<qsizetype>(sv.size()));
    }
    case TransportRole:
        return static_cast<int>(dev.transport);
    case TransportLabelRole:
        return transportLabel(dev.transport);
    case PowerAvailableRole:
        return dev.powerAvailable;
    case IsStubRole:
        return !dev.powerAvailable;
    case PercentageRole:
        return pct.value_or(-1.0);
    case HasPercentageRole:
        return pct.has_value();
    case PercentageTextRole:
        return QString::fromStdString(core::formatPercentage(pct, coarse));
    case ChargingStateRole:
        return static_cast<int>(charging);
    case IsChargingRole:
        return charging == core::ChargingState::Charging;
    case ChargingLabelRole:
        return chargingLabel(charging);
    case SeverityRole:
        return severityString(severity);
    case BatteryIconRole: {
        const auto sv = core::batteryIconName(pct, charging);
        return QString::fromUtf8(sv.data(), static_cast<qsizetype>(sv.size()));
    }
    case SourceRole:
        return QString::fromStdString(dev.source);
    case UnavailableLabelRole:
        return unavailableLabel(dev.unavailableReason);
    case CellsRole:
        return cellsToVariantList(dev);
    case CellCountRole:
        return static_cast<int>(dev.cells.size());
    case IsPrimaryRole:
        return !m_summary.primaryId.empty() && dev.id == m_summary.primaryId;
    default:
        return {};
    }
}

QHash<int, QByteArray> BatteryDeviceModel::roleNames() const {
    return {
        {DeviceIdRole, "deviceId"},
        {NameRole, "name"},
        {VendorRole, "vendor"},
        {ModelRole, "model"},
        {CategoryRole, "category"},
        {CategoryIconRole, "categoryIcon"},
        {TransportRole, "transport"},
        {TransportLabelRole, "transportLabel"},
        {PowerAvailableRole, "powerAvailable"},
        {IsStubRole, "isStub"},
        {PercentageRole, "percentage"},
        {HasPercentageRole, "hasPercentage"},
        {PercentageTextRole, "percentageText"},
        {ChargingStateRole, "chargingState"},
        {IsChargingRole, "isCharging"},
        {ChargingLabelRole, "chargingLabel"},
        {SeverityRole, "severity"},
        {BatteryIconRole, "batteryIcon"},
        {SourceRole, "source"},
        {UnavailableLabelRole, "unavailableLabel"},
        {CellsRole, "cells"},
        {CellCountRole, "cellCount"},
        {IsPrimaryRole, "isPrimary"},
    };
}

void BatteryDeviceModel::setRawDevices(std::vector<core::BatteryDevice> devices) {
    m_rawDevices = std::move(devices);
    rebuildFiltered();
}

void BatteryDeviceModel::setFilterOptions(
    const core::FilterOptions &options,
    double lowThreshold,
    double criticalThreshold
) {
    m_filterOptions = options;
    m_lowThreshold = lowThreshold;
    m_criticalThreshold = criticalThreshold;
    rebuildFiltered();
}

void BatteryDeviceModel::setRefreshing(bool refreshing) {
    if (m_refreshing == refreshing) {
        return;
    }
    m_refreshing = refreshing;
    Q_EMIT refreshingChanged();
}

QString BatteryDeviceModel::primarySeverity() const {
    return severityString(m_summary.primarySeverity);
}

QString BatteryDeviceModel::primaryCategoryIcon() const {
    const auto sv = core::categoryIconName(m_summary.primaryCategory);
    return QString::fromUtf8(sv.data(), static_cast<qsizetype>(sv.size()));
}

QString BatteryDeviceModel::primaryBatteryIcon() const {
    const auto sv = core::batteryIconName(m_summary.primaryPercentage, m_summary.primaryCharging);
    return QString::fromUtf8(sv.data(), static_cast<qsizetype>(sv.size()));
}

void BatteryDeviceModel::rebuildFiltered() {
    auto next = core::sortAndFilterDevices(m_rawDevices, m_filterOptions);
    auto nextSummary = core::computeSummary(
        next,
        m_lowThreshold,
        m_criticalThreshold,
        m_filterOptions.preferredDeviceId
    );

    beginResetModel();
    m_filtered = std::move(next);
    m_summary = std::move(nextSummary);
    endResetModel();
    Q_EMIT summaryChanged();
}

} // namespace fluctus::battery::backend
