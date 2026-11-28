// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "core/BatteryLogic.hpp"
#include "core/BatteryTypes.hpp"

#include <QAbstractListModel>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVariantList>

#include <vector>

namespace fluctus::battery::backend {

/// Parses a UTF-8 JSON array produced by `scintilla_client_discover_json()`.
[[nodiscard]] std::vector<core::BatteryDevice> parseScintillaJson(const QByteArray &json);

/// QAbstractListModel exposing discovered battery devices, multi-cell telemetry,
/// and aggregate summary properties to QML.
class BatteryDeviceModel final : public QAbstractListModel {
    Q_OBJECT

    Q_PROPERTY(int count READ rowCount NOTIFY summaryChanged)
    Q_PROPERTY(int batteryCount READ batteryCount NOTIFY summaryChanged)
    Q_PROPERTY(bool hasAnyBattery READ hasAnyBattery NOTIFY summaryChanged)
    Q_PROPERTY(QString primaryId READ primaryId NOTIFY summaryChanged)
    Q_PROPERTY(QString primaryName READ primaryName NOTIFY summaryChanged)
    Q_PROPERTY(double primaryPercentage READ primaryPercentage NOTIFY summaryChanged)
    Q_PROPERTY(bool hasPrimaryPercentage READ hasPrimaryPercentage NOTIFY summaryChanged)
    Q_PROPERTY(QString primaryPercentageText READ primaryPercentageText NOTIFY summaryChanged)
    Q_PROPERTY(bool primaryCharging READ primaryCharging NOTIFY summaryChanged)
    Q_PROPERTY(QString primarySeverity READ primarySeverity NOTIFY summaryChanged)
    Q_PROPERTY(QString primaryCategoryIcon READ primaryCategoryIcon NOTIFY summaryChanged)
    Q_PROPERTY(QString primaryBatteryIcon READ primaryBatteryIcon NOTIFY summaryChanged)
    Q_PROPERTY(int lowCount READ lowCount NOTIFY summaryChanged)
    Q_PROPERTY(int chargingCount READ chargingCount NOTIFY summaryChanged)
    Q_PROPERTY(bool refreshing READ refreshing NOTIFY refreshingChanged)
    /// Devices whose battery needs a udev rule to be read (see the Devices settings page).
    Q_PROPERTY(QStringList permissionNeededNames READ permissionNeededNames NOTIFY summaryChanged)

public:
    enum Roles : int {
        DeviceIdRole = Qt::UserRole + 1,
        NameRole,
        VendorRole,
        ModelRole,
        CategoryRole,
        CategoryIconRole,
        TransportRole,
        TransportLabelRole,
        PowerAvailableRole,
        IsStubRole,
        PercentageRole,
        HasPercentageRole,
        PercentageTextRole,
        ChargingStateRole,
        IsChargingRole,
        ChargingLabelRole,
        SeverityRole,
        BatteryIconRole,
        SourceRole,
        UnavailableLabelRole,
        CellsRole,
        CellCountRole,
        IsPrimaryRole,
        NeedsPermissionRole,
    };
    Q_ENUM(Roles)

    explicit BatteryDeviceModel(QObject *parent = nullptr);
    ~BatteryDeviceModel() override = default;

    [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void setRawDevices(std::vector<core::BatteryDevice> devices);
    void setFilterOptions(
        const core::FilterOptions &options,
        double lowThreshold,
        double criticalThreshold
    );
    void setRefreshing(bool refreshing);

    [[nodiscard]] int batteryCount() const noexcept { return static_cast<int>(m_summary.batteryCount); }
    [[nodiscard]] bool hasAnyBattery() const noexcept { return m_summary.hasAnyBattery; }
    [[nodiscard]] QString primaryId() const { return QString::fromStdString(m_summary.primaryId); }
    [[nodiscard]] QString primaryName() const { return QString::fromStdString(m_summary.primaryName); }
    [[nodiscard]] double primaryPercentage() const noexcept {
        return m_summary.primaryPercentage.value_or(-1.0);
    }
    [[nodiscard]] bool hasPrimaryPercentage() const noexcept {
        return m_summary.primaryPercentage.has_value();
    }
    [[nodiscard]] QString primaryPercentageText() const {
        return QString::fromStdString(
            core::formatPercentage(m_summary.primaryPercentage, m_summary.primaryCoarse)
        );
    }
    [[nodiscard]] bool primaryCharging() const noexcept {
        return m_summary.primaryCharging == core::ChargingState::Charging;
    }
    [[nodiscard]] QString primarySeverity() const;
    [[nodiscard]] QString primaryCategoryIcon() const;
    [[nodiscard]] QString primaryBatteryIcon() const;
    [[nodiscard]] int lowCount() const noexcept { return static_cast<int>(m_summary.lowCount); }
    [[nodiscard]] int chargingCount() const noexcept { return static_cast<int>(m_summary.chargingCount); }
    [[nodiscard]] bool refreshing() const noexcept { return m_refreshing; }
    [[nodiscard]] QStringList permissionNeededNames() const { return m_permissionNeeded; }

Q_SIGNALS:
    void summaryChanged();
    void refreshingChanged();

private:
    void rebuildFiltered();

    std::vector<core::BatteryDevice> m_rawDevices;
    std::vector<core::BatteryDevice> m_filtered;
    core::FilterOptions m_filterOptions;
    double m_lowThreshold{20.0};
    double m_criticalThreshold{10.0};
    core::BatterySummary m_summary;
    bool m_refreshing{false};
    QStringList m_permissionNeeded;
};

} // namespace fluctus::battery::backend
