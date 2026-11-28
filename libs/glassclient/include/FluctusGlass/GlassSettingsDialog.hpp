// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
//
// Modal Qt Widgets dialog letting any application customize Fluctus Liquid Glass for its own
// surfaces: preset selector (Global Default / Light / Medium / Full / Crystal / Plain / Off),
// Reset to Global Defaults, and full material + pointer sliders with live preview.
#pragma once

#include "FluctusGlass/GlassProfile.hpp"

#include <QDialog>
#include <QString>

#include <optional>

class QButtonGroup;
class QPushButton;

namespace fluctus::glass {

class GlassMaterialWidget;

class GlassSettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit GlassSettingsDialog(QWidget *parent = nullptr);
    ~GlassSettingsDialog() override;

    void setProfileJson(const QString &json);
    [[nodiscard]] QString profileJson() const;

    [[nodiscard]] GlassProfile *profile() { return &m_profile; }
    [[nodiscard]] const GlassProfile *profile() const { return &m_profile; }

    /// Convenience modal helper: returns the accepted profile JSON, or std::nullopt on Cancel.
    [[nodiscard]] static std::optional<QString> editProfile(const QString &initialJson, QWidget *parent = nullptr);

Q_SIGNALS:
    void profileChanged(const QString &json);

protected:
    void reject() override;

private:
    void syncFromProfile();

    GlassProfile m_profile;
    QString m_initialJson;
    QButtonGroup *m_presetButtons{nullptr};
    QPushButton *m_resetButton{nullptr};
    GlassMaterialWidget *m_editor{nullptr};
    bool m_syncing{false};
};

} // namespace fluctus::glass
