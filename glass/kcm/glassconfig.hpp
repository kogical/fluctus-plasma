// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <KCModule>

namespace fluctus::glass {

class GlassEffectConfig : public KCModule {
    Q_OBJECT

public:
    explicit GlassEffectConfig(QObject *parent, const KPluginMetaData &data);

    void save() override;
};

} // namespace fluctus::glass
