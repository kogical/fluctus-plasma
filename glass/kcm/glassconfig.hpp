// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <KCModule>

namespace fluctus::glass {

class GlassMaterialWidget;

class GlassEffectConfig : public KCModule {
    Q_OBJECT

public:
    explicit GlassEffectConfig(QObject *parent, const KPluginMetaData &data);

    void load() override;
    void save() override;
    void defaults() override;

private:
    GlassMaterialWidget *m_materialEditor{nullptr};
};

} // namespace fluctus::glass
