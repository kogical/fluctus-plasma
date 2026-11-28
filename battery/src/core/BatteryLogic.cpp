// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0

#include "core/BatteryLogic.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace fluctus::battery::core {

namespace {

[[nodiscard]] bool isPrimaryDeviceCategory(DeviceCategory category) noexcept {
    return category == DeviceCategory::Laptop || category == DeviceCategory::Ups;
}

[[nodiscard]] int categoryRank(DeviceCategory category) noexcept {
    switch (category) {
    case DeviceCategory::Laptop:
        return 0;
    case DeviceCategory::Ups:
        return 1;
    case DeviceCategory::Phone:
    case DeviceCategory::Tablet:
        return 2;
    case DeviceCategory::Keyboard:
    case DeviceCategory::Mouse:
        return 3;
    case DeviceCategory::Earbuds:
    case DeviceCategory::Headset:
    case DeviceCategory::Headphones:
    case DeviceCategory::Speaker:
        return 4;
    case DeviceCategory::Controller:
    case DeviceCategory::Wearable:
    case DeviceCategory::Stylus:
        return 5;
    case DeviceCategory::Unknown:
        return 6;
    case DeviceCategory::LinePower:
        return 7;
    }
    return 6;
}

} // namespace

std::optional<double> primaryPercentage(const BatteryDevice &device) noexcept {
    if (!device.powerAvailable || device.cells.empty()) {
        return std::nullopt;
    }

    for (const auto &cell : device.cells) {
        if (cell.role == CellRole::Primary && cell.percentage.has_value()) {
            return std::clamp(*cell.percentage, 0.0, 100.0);
        }
    }

    // For multi-cell devices without a Primary cell (e.g. Left/Right earbuds or split keyboard),
    // report the minimum of Left/Right so the user sees whichever side will die first.
    double minLr = std::numeric_limits<double>::infinity();
    bool foundLr = false;
    for (const auto &cell : device.cells) {
        if ((cell.role == CellRole::Left || cell.role == CellRole::Right) && cell.percentage.has_value()) {
            minLr = std::min(minLr, std::clamp(*cell.percentage, 0.0, 100.0));
            foundLr = true;
        }
    }
    if (foundLr) {
        return minLr;
    }

    for (const auto &cell : device.cells) {
        if (cell.percentage.has_value()) {
            return std::clamp(*cell.percentage, 0.0, 100.0);
        }
    }

    for (const auto &cell : device.cells) {
        if (const auto approx = coarseToPercentage(cell.coarse)) {
            return approx;
        }
    }

    return std::nullopt;
}

CoarseLevel primaryCoarseLevel(const BatteryDevice &device) noexcept {
    if (!device.powerAvailable) {
        return CoarseLevel::None;
    }
    for (const auto &cell : device.cells) {
        if (cell.coarse != CoarseLevel::None) {
            return cell.coarse;
        }
    }
    return CoarseLevel::None;
}

ChargingState aggregateChargingState(const BatteryDevice &device) noexcept {
    if (!device.powerAvailable || device.cells.empty()) {
        return ChargingState::Unknown;
    }

    bool anyCharging = false;
    bool anyDischarging = false;
    bool anyNotCharging = false;
    bool allFull = true;

    for (const auto &cell : device.cells) {
        if (cell.charging == ChargingState::Charging) {
            anyCharging = true;
        } else if (cell.charging == ChargingState::Discharging) {
            anyDischarging = true;
        } else if (cell.charging == ChargingState::NotCharging) {
            anyNotCharging = true;
        }
        if (cell.charging != ChargingState::Full) {
            allFull = false;
        }
    }

    if (anyCharging) {
        return ChargingState::Charging;
    }
    if (allFull) {
        return ChargingState::Full;
    }
    if (anyDischarging) {
        return ChargingState::Discharging;
    }
    if (anyNotCharging) {
        return ChargingState::NotCharging;
    }
    return ChargingState::Unknown;
}

std::string formatPercentage(std::optional<double> percentage, CoarseLevel coarse) {
    if (percentage.has_value() && std::isfinite(*percentage)) {
        const auto rounded = static_cast<int>(std::lround(std::clamp(*percentage, 0.0, 100.0)));
        return std::to_string(rounded) + "%";
    }
    switch (coarse) {
    case CoarseLevel::Critical:
        return "Crit";
    case CoarseLevel::Low:
        return "Low";
    case CoarseLevel::Normal:
        return "Mid";
    case CoarseLevel::High:
        return "High";
    case CoarseLevel::Full:
        return "Full";
    case CoarseLevel::None:
    default:
        return "—";
    }
}

DeviceCategory parseCategory(std::string_view value) noexcept {
    if (value == "laptop") return DeviceCategory::Laptop;
    if (value == "line_power") return DeviceCategory::LinePower;
    if (value == "ups") return DeviceCategory::Ups;
    if (value == "mouse") return DeviceCategory::Mouse;
    if (value == "keyboard") return DeviceCategory::Keyboard;
    if (value == "headset") return DeviceCategory::Headset;
    if (value == "earbuds") return DeviceCategory::Earbuds;
    if (value == "headphones") return DeviceCategory::Headphones;
    if (value == "speaker") return DeviceCategory::Speaker;
    if (value == "controller") return DeviceCategory::Controller;
    if (value == "tablet") return DeviceCategory::Tablet;
    if (value == "phone") return DeviceCategory::Phone;
    if (value == "wearable") return DeviceCategory::Wearable;
    if (value == "stylus") return DeviceCategory::Stylus;
    return DeviceCategory::Unknown;
}

Transport parseTransport(std::string_view value) noexcept {
    if (value == "internal") return Transport::Internal;
    if (value == "usb") return Transport::Usb;
    if (value == "wireless_dongle") return Transport::WirelessDongle;
    if (value == "bluetooth_classic") return Transport::BluetoothClassic;
    if (value == "ble") return Transport::Ble;
    return Transport::Unknown;
}

CellRole parseCellRole(std::string_view value) noexcept {
    if (value == "primary") return CellRole::Primary;
    if (value == "left") return CellRole::Left;
    if (value == "right") return CellRole::Right;
    if (value == "case") return CellRole::Case;
    if (value == "dock") return CellRole::Dock;
    if (value == "spare") return CellRole::Spare;
    return CellRole::Primary;
}

ChargingState parseChargingState(std::string_view value) noexcept {
    if (value == "charging") return ChargingState::Charging;
    if (value == "discharging") return ChargingState::Discharging;
    if (value == "full") return ChargingState::Full;
    if (value == "not_charging") return ChargingState::NotCharging;
    return ChargingState::Unknown;
}

CoarseLevel parseCoarseLevel(std::string_view value) noexcept {
    if (value == "critical") return CoarseLevel::Critical;
    if (value == "low") return CoarseLevel::Low;
    if (value == "normal") return CoarseLevel::Normal;
    if (value == "high") return CoarseLevel::High;
    if (value == "full") return CoarseLevel::Full;
    return CoarseLevel::None;
}

UnavailableReason parseUnavailableReason(std::string_view value) noexcept {
    if (value == "no_battery") return UnavailableReason::NoBattery;
    if (value == "unsupported_protocol") return UnavailableReason::UnsupportedProtocol;
    if (value == "permission_denied") return UnavailableReason::PermissionDenied;
    if (value == "stack_feature_disabled") return UnavailableReason::StackFeatureDisabled;
    if (value == "device_asleep") return UnavailableReason::DeviceAsleep;
    if (value == "read_error") return UnavailableReason::ReadError;
    return UnavailableReason::None;
}

std::vector<BatteryDevice> sortAndFilterDevices(
    std::span<const BatteryDevice> devices,
    const FilterOptions &options
) {
    std::vector<BatteryDevice> out;
    out.reserve(devices.size());

    for (const auto &dev : devices) {
        if (options.hideLinePower && dev.category == DeviceCategory::LinePower) {
            continue;
        }
        if (!options.showStubs && !dev.powerAvailable) {
            continue;
        }
        out.push_back(dev);
    }

    auto comparator = [&options](const BatteryDevice &a, const BatteryDevice &b) {
        const bool aPref = !options.preferredDeviceId.empty() && a.id == options.preferredDeviceId;
        const bool bPref = !options.preferredDeviceId.empty() && b.id == options.preferredDeviceId;
        if (aPref != bPref) {
            return aPref;
        }

        // Readable batteries always precede stub devices.
        if (a.powerAvailable != b.powerAvailable) {
            return a.powerAvailable;
        }

        const auto aPct = primaryPercentage(a);
        const auto bPct = primaryPercentage(b);

        if (options.sortOrder == SortOrder::PrimaryThenLowest) {
            const bool aPrimary = isPrimaryDeviceCategory(a.category);
            const bool bPrimary = isPrimaryDeviceCategory(b.category);
            if (aPrimary != bPrimary) {
                return aPrimary;
            }
            if (aPct.has_value() != bPct.has_value()) {
                return aPct.has_value();
            }
            if (aPct.has_value() && bPct.has_value() && std::abs(*aPct - *bPct) > 0.05) {
                return *aPct < *bPct;
            }
            const int aRank = categoryRank(a.category);
            const int bRank = categoryRank(b.category);
            if (aRank != bRank) {
                return aRank < bRank;
            }
        } else if (options.sortOrder == SortOrder::LowestFirst) {
            if (aPct.has_value() != bPct.has_value()) {
                return aPct.has_value();
            }
            if (aPct.has_value() && bPct.has_value() && std::abs(*aPct - *bPct) > 0.05) {
                return *aPct < *bPct;
            }
        }

        if (a.name != b.name) {
            return a.name < b.name;
        }
        return a.id < b.id;
    };

    std::ranges::stable_sort(out, comparator);
    return out;
}

BatterySummary computeSummary(
    std::span<const BatteryDevice> filteredDevices,
    double lowThreshold,
    double criticalThreshold,
    std::string_view preferredDeviceId
) {
    BatterySummary summary;
    summary.totalCount = filteredDevices.size();

    const BatteryDevice *headline = nullptr;

    for (const auto &dev : filteredDevices) {
        if (!dev.powerAvailable) {
            continue;
        }
        ++summary.batteryCount;
        summary.hasAnyBattery = true;

        const auto pct = primaryPercentage(dev);
        const auto charging = aggregateChargingState(dev);
        if (charging == ChargingState::Charging) {
            ++summary.chargingCount;
        }
        if (pct.has_value()) {
            if (!summary.lowestPercentage.has_value() || *pct < *summary.lowestPercentage) {
                summary.lowestPercentage = pct;
            }
            if (charging != ChargingState::Charging && *pct <= lowThreshold) {
                ++summary.lowCount;
            }
        }

        if ((!preferredDeviceId.empty() && dev.id == preferredDeviceId) || headline == nullptr) {
            headline = &dev;
        }
    }

    if (headline != nullptr) {
        summary.primaryId = headline->id;
        summary.primaryName = headline->name;
        summary.primaryCategory = headline->category;
        summary.primaryPercentage = primaryPercentage(*headline);
        summary.primaryCoarse = primaryCoarseLevel(*headline);
        summary.primaryCharging = aggregateChargingState(*headline);
        summary.primarySeverity = classifySeverity(
            summary.primaryPercentage,
            summary.primaryCharging,
            lowThreshold,
            criticalThreshold
        );
    }

    return summary;
}

} // namespace fluctus::battery::core
