// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0

#include "FluctusGlass/GlassSettingsDialog.hpp"
#include "FluctusGlass/GlassMaterialWidget.hpp"
#include "FluctusGlass/GlassProtocol.hpp"

#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace fluctus::glass {

namespace {

struct PresetEntry {
    const char *id;
    const char *label;
};

constexpr PresetEntry kPresetEntries[] = {
    {"global", "Global Default"},
    {"light", "Light"},
    {"medium", "Medium"},
    {"full", "Full"},
    {"crystal", "Crystal"},
    {"plain", "Plain Blur"},
    {"none", "Off"},
};

} // namespace

GlassSettingsDialog::GlassSettingsDialog(QWidget *parent)
    : QDialog(parent) {
    setWindowTitle(tr("Liquid Glass Settings"));
    resize(460, 620);

    auto *root = new QVBoxLayout(this);

    auto *intro = new QLabel(
        tr("Customize Liquid Glass for this app only. Choose a preset or adjust the sliders below; "
           "Global Default tracks your Desktop Effects settings automatically."),
        this);
    intro->setWordWrap(true);
    root->addWidget(intro);

    auto *presetGroup = new QGroupBox(tr("Preset"), this);
    auto *presetGrid = new QGridLayout(presetGroup);
    m_presetButtons = new QButtonGroup(this);
    m_presetButtons->setExclusive(true);

    int index = 0;
    for (const PresetEntry &entry : kPresetEntries) {
        auto *button = new QPushButton(tr(entry.label), presetGroup);
        button->setCheckable(true);
        button->setProperty("presetId", QString::fromLatin1(entry.id));
        m_presetButtons->addButton(button, index);
        presetGrid->addWidget(button, index / 4, index % 4);
        ++index;
    }

    connect(m_presetButtons, &QButtonGroup::idClicked, this, [this](int id) {
        if (m_syncing) {
            return;
        }
        if (QAbstractButton *btn = m_presetButtons->button(id)) {
            m_profile.applyPreset(btn->property("presetId").toString());
        }
    });
    root->addWidget(presetGroup);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    m_editor = new GlassMaterialWidget(scroll);
    scroll->setWidget(m_editor);
    root->addWidget(scroll, 1);

    auto *footer = new QHBoxLayout();
    m_resetButton = new QPushButton(tr("Reset to Global Defaults"), this);
    connect(m_resetButton, &QPushButton::clicked, this, [this]() {
        m_profile.resetToGlobal();
    });
    footer->addWidget(m_resetButton);
    footer->addStretch(1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &GlassSettingsDialog::reject);
    footer->addWidget(buttons);
    root->addLayout(footer);

    connect(m_editor, &GlassMaterialWidget::parametersChanged, this, [this](const GlassParameters &params) {
        if (!m_syncing) {
            m_profile.setParameters(params);
        }
    });
    connect(&m_profile, &GlassProfile::presetChanged, this, &GlassSettingsDialog::syncFromProfile);
    connect(&m_profile, &GlassProfile::parametersChanged, this, &GlassSettingsDialog::syncFromProfile);
    connect(&m_profile, &GlassProfile::jsonChanged, this, [this]() {
        if (!m_syncing) {
            Q_EMIT profileChanged(m_profile.toJson());
        }
    });

    m_initialJson = m_profile.toJson();
    syncFromProfile();
}

GlassSettingsDialog::~GlassSettingsDialog() = default;

void GlassSettingsDialog::setProfileJson(const QString &json) {
    m_syncing = true;
    m_profile.loadJson(json);
    m_initialJson = m_profile.toJson();
    m_syncing = false;
    syncFromProfile();
}

QString GlassSettingsDialog::profileJson() const {
    return m_profile.toJson();
}

std::optional<QString> GlassSettingsDialog::editProfile(const QString &initialJson, QWidget *parent) {
    GlassSettingsDialog dialog(parent);
    dialog.setProfileJson(initialJson);
    if (dialog.exec() == QDialog::Accepted) {
        return dialog.profileJson();
    }
    return std::nullopt;
}

void GlassSettingsDialog::reject() {
    if (m_profile.toJson() != m_initialJson) {
        m_profile.loadJson(m_initialJson);
        Q_EMIT profileChanged(m_initialJson);
    }
    QDialog::reject();
}

void GlassSettingsDialog::syncFromProfile() {
    m_syncing = true;
    const QString active = m_profile.preset();
    bool matched = false;
    for (QAbstractButton *btn : m_presetButtons->buttons()) {
        const bool isMatch = btn->property("presetId").toString() == active;
        if (isMatch) {
            btn->setChecked(true);
            matched = true;
        }
    }
    if (!matched) {
        // Custom tuning: uncheck all preset buttons.
        m_presetButtons->setExclusive(false);
        for (QAbstractButton *btn : m_presetButtons->buttons()) {
            btn->setChecked(false);
        }
        m_presetButtons->setExclusive(true);
    }

    const bool opticalEnabled = m_profile.mode() == protocol::param::kModeGlass;
    m_editor->setEnabled(opticalEnabled);
    m_editor->setParameters(m_profile.parameters());
    m_resetButton->setEnabled(!m_profile.followsGlobal());
    m_syncing = false;
}

} // namespace fluctus::glass
