// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace fluctus::battery::core {

enum class DeviceCategory : std::uint8_t {
    Laptop,
    LinePower,
    Ups,
    Mouse,
    Keyboard,
    Headset,
    Earbuds,
    Headphones,
    Speaker,
    Controller,
    Tablet,
    Phone,
    Wearable,
    Stylus,
    Unknown,
};

enum class Transport : std::uint8_t {
    Internal,
    Usb,
    WirelessDongle,
    BluetoothClassic,
    Ble,
    Unknown,
};

enum class CellRole : std::uint8_t {
    Primary,
    Left,
    Right,
    Case,
    Dock,
    Spare,
};

enum class ChargingState : std::uint8_t {
    Charging,
    Discharging,
    Full,
    NotCharging,
    Unknown,
};

enum class CoarseLevel : std::uint8_t {
    None,
    Critical,
    Low,
    Normal,
    High,
    Full,
};

enum class UnavailableReason : std::uint8_t {
    None,
    NoBattery,
    UnsupportedProtocol,
    PermissionDenied,
    StackFeatureDisabled,
    DeviceAsleep,
    ReadError,
};

enum class BatterySeverity : std::uint8_t {
    Unknown,
    Normal,
    Warning,
    Critical,
    Charging,
    Full,
};

enum class SortOrder : std::uint8_t {
    PrimaryThenLowest,
    LowestFirst,
    Alphabetical,
};

struct BatteryCell {
    CellRole role{CellRole::Primary};
    std::optional<double> percentage;
    CoarseLevel coarse{CoarseLevel::None};
    ChargingState charging{ChargingState::Unknown};
    std::optional<std::uint32_t> voltageMv;
};

struct BatteryDevice {
    std::string id;
    std::string name;
    std::optional<std::string> vendor;
    std::optional<std::string> model;
    DeviceCategory category{DeviceCategory::Unknown};
    Transport transport{Transport::Unknown};
    bool powerAvailable{false};
    std::string source;
    UnavailableReason unavailableReason{UnavailableReason::None};
    std::vector<BatteryCell> cells;
};

struct FilterOptions {
    bool showStubs{false};
    bool hideLinePower{true};
    SortOrder sortOrder{SortOrder::PrimaryThenLowest};
    std::string preferredDeviceId;
};

struct BatterySummary {
    bool hasAnyBattery{false};
    std::size_t totalCount{0};
    std::size_t batteryCount{0};
    std::size_t chargingCount{0};
    std::size_t lowCount{0};
    std::string primaryId;
    std::string primaryName;
    DeviceCategory primaryCategory{DeviceCategory::Unknown};
    std::optional<double> primaryPercentage;
    CoarseLevel primaryCoarse{CoarseLevel::None};
    ChargingState primaryCharging{ChargingState::Unknown};
    BatterySeverity primarySeverity{BatterySeverity::Unknown};
    std::optional<double> lowestPercentage;
};

} // namespace fluctus::battery::core
