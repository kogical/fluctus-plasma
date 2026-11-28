// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "FluctusGlass/GlassProfile.hpp"

#include <QtQml/qqmlregistration.h>

namespace fluctus::glass {

class GlassProfileQml : public GlassProfile {
    Q_OBJECT
    QML_NAMED_ELEMENT(GlassProfile)

public:
    using GlassProfile::GlassProfile;
};

} // namespace fluctus::glass
