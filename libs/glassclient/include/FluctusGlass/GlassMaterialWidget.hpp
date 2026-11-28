// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
//
// Reusable Qt Widgets editor for Liquid Glass material and pointer parameters.
// Used both by GlassSettingsDialog (per-app modal) and by Fluctus Glass's Desktop Effects KCM.
#pragma once

#include "FluctusGlass/GlassRequest.hpp"

#include <QColor>
#include <QWidget>

class QCheckBox;
class QFormLayout;
class QGroupBox;
class QLabel;
class QPushButton;
class QSlider;

namespace fluctus::glass {

class GlassMaterialWidget : public QWidget {
    Q_OBJECT

public:
    explicit GlassMaterialWidget(QWidget *parent = nullptr);
    ~GlassMaterialWidget() override;

    void setParameters(const GlassParameters &parameters);
    [[nodiscard]] GlassParameters parameters() const;

    void setMaterialGroupTitle(const QString &title);
    void setPointerGroupTitle(const QString &title);

Q_SIGNALS:
    void parametersChanged(const fluctus::glass::GlassParameters &parameters);

private:
    struct SliderRow {
        QSlider *slider{nullptr};
        QLabel *readout{nullptr};
    };

    SliderRow addSliderRow(QFormLayout *form, const QString &label, int min, int max, const QString &suffix);
    void updateTintButton();
    void emitIfInteractive();

    GlassParameters m_base;
    QGroupBox *m_materialGroup{nullptr};
    QGroupBox *m_pointerGroup{nullptr};

    SliderRow m_intensity;
    SliderRow m_refraction;
    SliderRow m_bezel;
    SliderRow m_magnify;
    SliderRow m_dispersion;
    SliderRow m_frost;
    SliderRow m_saturation;
    SliderRow m_brightness;
    QPushButton *m_tintButton{nullptr};
    QColor m_tintColor{Qt::white};
    SliderRow m_tintStrength;
    SliderRow m_highlight;
    SliderRow m_edgeShade;
    SliderRow m_sheen;

    QCheckBox *m_pointerEffects{nullptr};
    SliderRow m_pointerBulge;

    bool m_updating{false};
};

} // namespace fluctus::glass
