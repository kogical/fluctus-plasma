// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "FluctusGlass/GlassProfile.hpp"

#include <QtQml/qqmlregistration.h>

namespace fluctus::glass {

struct GlassProfileForeign {
    Q_GADGET
    QML_FOREIGN(fluctus::glass::GlassProfile)
    QML_NAMED_ELEMENT(GlassProfile)
};

} // namespace fluctus::glass
