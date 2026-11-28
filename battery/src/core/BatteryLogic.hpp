// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "core/BatteryTypes.hpp"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fluctus::battery::core {

/// Converts a discrete coarse level into an approximate display percentage.
[[nodiscard]] constexpr std::optional<double> coarseToPercentage(CoarseLevel level) noexcept {
    switch (level) {
    case CoarseLevel::Critical:
        return 5.0;
    case CoarseLevel::Low:
        return 20.0;
    case CoarseLevel::Normal:
        return 55.0;
    case CoarseLevel::High:
        return 80.0;
    case CoarseLevel::Full:
        return 100.0;
    case CoarseLevel::None:
    default:
        return std::nullopt;
    }
}

/// Computes the headline percentage for a device (primary cell, or lowest active
/// wearable cell such as Left/Right earbuds so the user isn't caught off guard).
[[nodiscard]] std::optional<double> primaryPercentage(const BatteryDevice &device) noexcept;

/// Returns the first coarse level when no exact percentage is available.
[[nodiscard]] CoarseLevel primaryCoarseLevel(const BatteryDevice &device) noexcept;

/// Aggregates the charging state across all cells of a device.
[[nodiscard]] ChargingState aggregateChargingState(const BatteryDevice &device) noexcept;

/// Classifies battery severity for accent color and alert badges.
[[nodiscard]] constexpr BatterySeverity classifySeverity(
    std::optional<double> percentage,
    ChargingState charging,
    double lowThreshold = 20.0,
    double criticalThreshold = 10.0
) noexcept {
    if (charging == ChargingState::Charging) {
        return BatterySeverity::Charging;
    }
    if (charging == ChargingState::Full) {
        return BatterySeverity::Full;
    }
    if (!percentage.has_value()) {
        return BatterySeverity::Unknown;
    }
    const double pct = *percentage;
    if (pct <= criticalThreshold) {
        return BatterySeverity::Critical;
    }
    if (pct <= lowThreshold) {
        return BatterySeverity::Warning;
    }
    if (pct >= 99.5) {
        return BatterySeverity::Full;
    }
    return BatterySeverity::Normal;
}

/// Returns a Freedesktop/Breeze symbolic icon name for a device category.
[[nodiscard]] constexpr std::string_view categoryIconName(DeviceCategory category) noexcept {
    switch (category) {
    case DeviceCategory::Laptop:
        return "computer-laptop-symbolic";
    case DeviceCategory::LinePower:
        return "ac-adapter-symbolic";
    case DeviceCategory::Ups:
        return "uninterruptible-power-supply-symbolic";
    case DeviceCategory::Mouse:
        return "input-mouse-symbolic";
    case DeviceCategory::Keyboard:
        return "input-keyboard-symbolic";
    case DeviceCategory::Headset:
        return "audio-headset-symbolic";
    case DeviceCategory::Earbuds:
    case DeviceCategory::Headphones:
        return "audio-headphones-symbolic";
    case DeviceCategory::Speaker:
        return "audio-speakers-symbolic";
    case DeviceCategory::Controller:
        return "input-gaming-symbolic";
    case DeviceCategory::Tablet:
        return "input-tablet-symbolic";
    case DeviceCategory::Phone:
        return "phone-symbolic";
    case DeviceCategory::Wearable:
        return "watch-symbolic";
    case DeviceCategory::Stylus:
        return "draw-freehand-symbolic";
    case DeviceCategory::Unknown:
    default:
        return "battery-good-symbolic";
    }
}

/// Returns a Freedesktop/Breeze battery level icon name for a percentage and charging state.
[[nodiscard]] constexpr std::string_view batteryIconName(
    std::optional<double> percentage,
    ChargingState charging
) noexcept {
    if (!percentage.has_value()) {
        return "battery-missing-symbolic";
    }
    const bool isCharging = charging == ChargingState::Charging;
    const double pct = *percentage;
    if (pct >= 95.0) {
        return isCharging ? "battery-100-charging" : "battery-100";
    }
    if (pct >= 80.0) {
        return isCharging ? "battery-080-charging" : "battery-080";
    }
    if (pct >= 60.0) {
        return isCharging ? "battery-060-charging" : "battery-060";
    }
    if (pct >= 40.0) {
        return isCharging ? "battery-040-charging" : "battery-040";
    }
    if (pct >= 20.0) {
        return isCharging ? "battery-020-charging" : "battery-020";
    }
    if (pct >= 10.0) {
        return isCharging ? "battery-010-charging" : "battery-010";
    }
    return isCharging ? "battery-000-charging" : "battery-caution";
}

/// Formats a percentage or coarse level for compact UI display (e.g. "83%", "Low", "—").
[[nodiscard]] std::string formatPercentage(std::optional<double> percentage, CoarseLevel coarse);

/// Parses enum identifiers emitted by scintilla's JSON snapshot.
[[nodiscard]] DeviceCategory parseCategory(std::string_view value) noexcept;
[[nodiscard]] Transport parseTransport(std::string_view value) noexcept;
[[nodiscard]] CellRole parseCellRole(std::string_view value) noexcept;
[[nodiscard]] ChargingState parseChargingState(std::string_view value) noexcept;
[[nodiscard]] CoarseLevel parseCoarseLevel(std::string_view value) noexcept;
[[nodiscard]] UnavailableReason parseUnavailableReason(std::string_view value) noexcept;

/// A device that has a battery we cannot read right now (needs udev access, or is asleep).
/// These stay visible even when devices without battery telemetry are hidden.
[[nodiscard]] bool hasBatteryButUnreadable(const BatteryDevice &device) noexcept;

/// Names of devices whose battery driver was denied access to `/dev/hidraw*` (missing udev
/// rule), deduplicated, in discovery order.
[[nodiscard]] std::vector<std::string> devicesNeedingPermission(std::span<const BatteryDevice> devices);

/// Filters and deterministically sorts discovered devices for UI presentation.
[[nodiscard]] std::vector<BatteryDevice> sortAndFilterDevices(
    std::span<const BatteryDevice> devices,
    const FilterOptions &options
);

/// Computes aggregate summary telemetry for the compact panel view and widget header.
[[nodiscard]] BatterySummary computeSummary(
    std::span<const BatteryDevice> filteredDevices,
    double lowThreshold = 20.0,
    double criticalThreshold = 10.0,
    std::string_view preferredDeviceId = {}
);

} // namespace fluctus::battery::core
