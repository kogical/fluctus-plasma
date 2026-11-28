// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0

#include "backend/BatteryDeviceModel.hpp"
#include "core/AdaptiveLayout.hpp"
#include "core/BatteryLogic.hpp"
#include "core/BatteryTypes.hpp"

#include <QtTest/QtTest>

using namespace fluctus::battery;

class TestBatteryCore : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void primaryPercentagePrefersPrimaryThenMinLR() {
        core::BatteryDevice earbuds;
        earbuds.powerAvailable = true;
        earbuds.cells = {
            core::BatteryCell{.role = core::CellRole::Left, .percentage = 84.0, .coarse = core::CoarseLevel::None, .charging = core::ChargingState::Discharging, .voltageMv = std::nullopt},
            core::BatteryCell{.role = core::CellRole::Right, .percentage = 62.0, .coarse = core::CoarseLevel::None, .charging = core::ChargingState::Discharging, .voltageMv = std::nullopt},
            core::BatteryCell{.role = core::CellRole::Case, .percentage = 95.0, .coarse = core::CoarseLevel::None, .charging = core::ChargingState::Full, .voltageMv = std::nullopt},
        };
        const auto pct = core::primaryPercentage(earbuds);
        QVERIFY(pct.has_value());
        QCOMPARE(*pct, 62.0);

        core::BatteryDevice coarseOnly;
        coarseOnly.powerAvailable = true;
        coarseOnly.cells = {
            core::BatteryCell{.role = core::CellRole::Primary, .percentage = std::nullopt, .coarse = core::CoarseLevel::Low, .charging = core::ChargingState::Unknown, .voltageMv = std::nullopt},
        };
        const auto coarsePct = core::primaryPercentage(coarseOnly);
        QVERIFY(coarsePct.has_value());
        QCOMPARE(*coarsePct, 20.0);
    }

    void severityAndFormatting() {
        QCOMPARE(core::classifySeverity(8.0, core::ChargingState::Discharging), core::BatterySeverity::Critical);
        QCOMPARE(core::classifySeverity(18.0, core::ChargingState::Discharging), core::BatterySeverity::Warning);
        QCOMPARE(core::classifySeverity(8.0, core::ChargingState::Charging), core::BatterySeverity::Charging);
        QCOMPARE(core::classifySeverity(100.0, core::ChargingState::Full), core::BatterySeverity::Full);
        QCOMPARE(core::classifySeverity(65.0, core::ChargingState::Discharging), core::BatterySeverity::Normal);
        QCOMPARE(core::classifySeverity(std::nullopt, core::ChargingState::Unknown), core::BatterySeverity::Unknown);

        QCOMPARE(core::formatPercentage(83.4, core::CoarseLevel::None), std::string("83%"));
        QCOMPARE(core::formatPercentage(std::nullopt, core::CoarseLevel::Low), std::string("Low"));
        QCOMPARE(core::formatPercentage(std::nullopt, core::CoarseLevel::None), std::string("—"));
    }

    void adaptiveLayoutBreakpointsAcrossResolutions() {
        // Tiny 24px top/bottom panel bar
        const auto micro = core::computeLayoutMetrics(120.0, 24.0, true, false);
        QCOMPARE(micro.mode, core::FormMode::MicroPanel);
        QVERIFY(!micro.showSecondaryLabel);
        QVERIFY(micro.fontSizePx >= 9.5 && micro.fontSizePx <= 13.0);
        QVERIFY(micro.iconSize >= 12.0 && micro.iconSize <= 20.0);

        // Standard 44px panel
        const auto standard = core::computeLayoutMetrics(140.0, 44.0, true, false);
        QCOMPARE(standard.mode, core::FormMode::CompactPanel);
        QVERIFY(standard.showSecondaryLabel);

        // Narrow 30px vertical panel
        const auto vertical = core::computeLayoutMetrics(30.0, 180.0, true, true);
        QCOMPARE(vertical.mode, core::FormMode::MicroPanel);
        QVERIFY(vertical.stackVertically);

        // Tiny desktop tile
        const auto tinyDesk = core::computeLayoutMetrics(150.0, 140.0, false, false);
        QCOMPARE(tinyDesk.mode, core::FormMode::DesktopTiny);
        QCOMPARE(tinyDesk.columns, 1);
        QVERIFY(tinyDesk.ringSize >= 42.0);

        // Compact desktop card
        const auto compactDesk = core::computeLayoutMetrics(280.0, 240.0, false, false);
        QCOMPARE(compactDesk.mode, core::FormMode::DesktopCompact);
        QCOMPARE(compactDesk.columns, 1);

        // Wide 4K desktop dashboard
        const auto wideDesk = core::computeLayoutMetrics(640.0, 320.0, false, false);
        QCOMPARE(wideDesk.mode, core::FormMode::DesktopExpanded);
        QVERIFY(wideDesk.columns >= 3 && wideDesk.columns <= 4);
    }

    void parsesScintillaJsonAndUpdatesModel() {
        const QByteArray json = R"([
            {
                "id": "sysfs:AC",
                "name": "AC Adapter",
                "vendor": null,
                "model": null,
                "category": "line_power",
                "transport": "internal",
                "available": false,
                "source": null,
                "unavailable_reason": "no_battery",
                "cells": []
            },
            {
                "id": "bt:11:22:33:44:55:66",
                "name": "Pro Earbuds",
                "vendor": "Acme",
                "model": "Buds 2",
                "category": "earbuds",
                "transport": "ble",
                "available": true,
                "source": "ble_gatt",
                "unavailable_reason": null,
                "cells": [
                    {"role": "left", "percentage": 40.0, "coarse": null, "charging": "discharging", "voltage_mv": null},
                    {"role": "right", "percentage": 35.0, "coarse": null, "charging": "discharging", "voltage_mv": null},
                    {"role": "case", "percentage": 90.0, "coarse": null, "charging": "charging", "voltage_mv": 4150}
                ]
            },
            {
                "id": "sysfs:BAT0",
                "name": "Laptop Battery",
                "vendor": "Lenovo",
                "model": "BAT0",
                "category": "laptop",
                "transport": "internal",
                "available": true,
                "source": "sysfs",
                "unavailable_reason": null,
                "cells": [
                    {"role": "primary", "percentage": 83.0, "coarse": null, "charging": "discharging", "voltage_mv": 12400}
                ]
            },
            {
                "id": "usb:1234:5678",
                "name": "Wired Keyboard",
                "vendor": null,
                "model": null,
                "category": "keyboard",
                "transport": "usb",
                "available": false,
                "source": null,
                "unavailable_reason": "no_battery",
                "cells": []
            }
        ])";

        const auto parsed = backend::parseScintillaJson(json);
        QCOMPARE(parsed.size(), std::size_t{4});

        backend::BatteryDeviceModel model;
        model.setRawDevices(parsed);

        // Default filter hides line_power and stubs -> 2 devices, Laptop first
        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.batteryCount(), 2);
        QVERIFY(model.hasAnyBattery());
        QCOMPARE(model.primaryName(), QStringLiteral("Laptop Battery"));
        QCOMPARE(model.primaryPercentageText(), QStringLiteral("83%"));
        QCOMPARE(model.chargingCount(), 1); // Earbuds case is charging

        const QModelIndex idx0 = model.index(0, 0);
        const QModelIndex idx1 = model.index(1, 0);
        QCOMPARE(model.data(idx0, backend::BatteryDeviceModel::NameRole).toString(), QStringLiteral("Laptop Battery"));
        QCOMPARE(model.data(idx1, backend::BatteryDeviceModel::NameRole).toString(), QStringLiteral("Pro Earbuds"));
        QCOMPARE(model.data(idx1, backend::BatteryDeviceModel::PercentageRole).toDouble(), 35.0);
        QCOMPARE(model.data(idx1, backend::BatteryDeviceModel::CellCountRole).toInt(), 3);

        // Enable stubs & pin earbuds as preferred device
        core::FilterOptions opts;
        opts.showStubs = true;
        opts.hideLinePower = true;
        opts.preferredDeviceId = "bt:11:22:33:44:55:66";
        model.setFilterOptions(opts, 20.0, 10.0);

        QCOMPARE(model.rowCount(), 3);
        QCOMPARE(model.primaryName(), QStringLiteral("Pro Earbuds"));
        QCOMPARE(model.primaryPercentageText(), QStringLiteral("35%"));

        // Malformed JSON safely yields empty vector
        QVERIFY(backend::parseScintillaJson(QByteArray("{not-an-array")).empty());
    }
};

QTEST_MAIN(TestBatteryCore)
#include "test_battery_core.moc"
