// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
//
// Wire contract between Fluctus Glass (the KWin effect) and its clients. Header-only and
// dependency-free apart from QtCore so the GPL effect and Apache-2.0 clients share one definition.
//
// Service   com.kogical.Fluctus.Glass         (owned by KWin while the effect is loaded)
// Object    /com/kogical/Fluctus/Glass
// Interface com.kogical.Fluctus.Glass1
//
//   Register(a{sv} match, a{sv} params) -> s handle
//   Set(s handle, a{sv} match, a{sv} params) -> s windowUuid (empty while unattached)
//   Unregister(s handle)
//   GetInfo() -> a{sv}                     { version: u, globalMode: b, capabilities: as }
//   signal Attached(s handle, s windowUuid)
//   signal Detached(s handle)
//
// The caller's process id comes from the D-Bus daemon, so a client can only request glass for
// windows of its own process. The shape is the window's own blur region (set with
// KWindowEffects::enableBlurBehind / org_kde_kwin_blur / ext-background-effect-v1), which is
// double-buffered with the surface and therefore never lags behind resizes. Explicit shapes
// can be supplied with the `shapes` parameter instead.
#pragma once

#include <QLatin1StringView>

namespace fluctus::glass::protocol {

inline constexpr unsigned int kVersion = 1;

inline constexpr QLatin1StringView kService{"com.kogical.Fluctus.Glass"};
inline constexpr QLatin1StringView kPath{"/com/kogical/Fluctus/Glass"};
inline constexpr QLatin1StringView kInterface{"com.kogical.Fluctus.Glass1"};

/// KWin plugin id of the effect (org.kde.kwin.Effects.isEffectLoaded / loadEffect).
inline constexpr QLatin1StringView kEffectId{"fluctusglass"};

/// Upper bound on live registrations per D-Bus connection.
inline constexpr int kMaxRegistrationsPerClient = 64;

namespace match {
/// int, logical px: the window's size. Strongly recommended when a process has many windows.
inline constexpr QLatin1StringView kWidth{"width"};
inline constexpr QLatin1StringView kHeight{"height"};
/// string: "dock", "normal", "dialog", "popup", "tooltip", "notification", "osd", "desktop".
inline constexpr QLatin1StringView kWindowType{"windowType"};
/// string: Wayland app id / X11 resource class (QGuiApplication::desktopFileName()).
inline constexpr QLatin1StringView kAppId{"appId"};
/// string: xdg-toplevel-tag.
inline constexpr QLatin1StringView kTag{"tag"};
/// string: exact window caption.
inline constexpr QLatin1StringView kTitle{"title"};
/// string: KWin's EffectWindow::internalId(), binds directly when already known.
inline constexpr QLatin1StringView kWindowUuid{"windowUuid"};
} // namespace match

namespace param {
// Behaviour
/// string: "glass" (default) renders Liquid Glass; "plain" keeps plain frosted blur for the
/// selected areas (opts them out of global mode); "none" draws no backdrop effect there at all.
inline constexpr QLatin1StringView kMode{"mode"};
/// double 0..2 (default 1): master strength scaling refraction, magnification, dispersion,
/// pointer bulge and highlights.
inline constexpr QLatin1StringView kIntensity{"intensity"};
/// string: "liquid" (default: diffused, magnified, vivid thick slab) or "crystal" (clear, deep,
/// sharp-edged). Applied first; every other key overrides it.
inline constexpr QLatin1StringView kPreset{"preset"};
inline constexpr QLatin1StringView kPresetLiquid{"liquid"};
inline constexpr QLatin1StringView kPresetCrystal{"crystal"};
inline constexpr QLatin1StringView kModeGlass{"glass"};
inline constexpr QLatin1StringView kModePlain{"plain"};
inline constexpr QLatin1StringView kModeNone{"none"};
// Shape
inline constexpr QLatin1StringView kRadius{"radius"};            // double, logical px
inline constexpr QLatin1StringView kInset{"inset"};              // double, px the blur region is grown by
inline constexpr QLatin1StringView kShapes{"shapes"};            // ad: x, y, w, h, radius per shape (window-local)
// Body
inline constexpr QLatin1StringView kTint{"tint"};                // string #AARRGGBB (alpha = tint strength)
inline constexpr QLatin1StringView kBezel{"bezel"};              // double, px
inline constexpr QLatin1StringView kRefraction{"refraction"};    // double, px
inline constexpr QLatin1StringView kMagnify{"magnify"};          // double 0..0.3, body magnification
inline constexpr QLatin1StringView kEdgeBend{"edgeBend"};        // double 0..1, steep bend at the rim
inline constexpr QLatin1StringView kDispersion{"dispersion"};    // double 0..1
inline constexpr QLatin1StringView kFrost{"frost"};              // double 0..1 (backdrop blur)
inline constexpr QLatin1StringView kSaturation{"saturation"};    // double multiplier
inline constexpr QLatin1StringView kBrightness{"brightness"};    // double multiplier
inline constexpr QLatin1StringView kEdgeShade{"edgeShade"};      // double 0..1
inline constexpr QLatin1StringView kGrain{"grain"};              // double 0..1
// Lighting
inline constexpr QLatin1StringView kHighlight{"highlight"};      // double, specular rim intensity
inline constexpr QLatin1StringView kHighlightWidth{"highlightWidth"}; // double, px
inline constexpr QLatin1StringView kSheen{"sheen"};              // double
inline constexpr QLatin1StringView kLightAngle{"lightAngle"};    // double, degrees (-90 = up)
// Borders and shadow
inline constexpr QLatin1StringView kInnerBorder{"innerBorder"};  // string #AARRGGBB
inline constexpr QLatin1StringView kOuterBorder{"outerBorder"};  // string #AARRGGBB
inline constexpr QLatin1StringView kBorderWidth{"borderWidth"};  // double, px
inline constexpr QLatin1StringView kShadowColor{"shadowColor"};  // string #AARRGGBB
inline constexpr QLatin1StringView kShadowBlur{"shadowBlur"};    // double, px
inline constexpr QLatin1StringView kShadowOffsetX{"shadowOffsetX"}; // double, px
inline constexpr QLatin1StringView kShadowOffsetY{"shadowOffsetY"}; // double, px
// Pointer
inline constexpr QLatin1StringView kPointerLight{"pointerLight"}; // double 0..1, light swing + spot
inline constexpr QLatin1StringView kPointerBulge{"pointerBulge"}; // double, px of lens bulge under the cursor
inline constexpr QLatin1StringView kPointerRadius{"pointerRadius"}; // double, px radius of the bulge
} // namespace param

} // namespace fluctus::glass::protocol
