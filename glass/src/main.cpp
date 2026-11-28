// SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: GPL-3.0-or-later

#include "glasseffect.hpp"

namespace KWin {

KWIN_EFFECT_FACTORY_SUPPORTED_ENABLED(fluctus::glass::GlassEffect,
    "metadata.json",
    return fluctus::glass::GlassEffect::supported();,
    return fluctus::glass::GlassEffect::enabledByDefault();)

} // namespace KWin

#include "main.moc"
