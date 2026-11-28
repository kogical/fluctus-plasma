// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0

#include "FluctusGlass/GlassMaterialWidget.hpp"

#include <QCheckBox>
#include <QColorDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fluctus::glass {

namespace {

int pct(qreal v) {
    return static_cast<int>(std::lround(v * 100.0));
}

int px(qreal v) {
    return static_cast<int>(std::lround(v));
}

} // namespace

GlassMaterialWidget::GlassMaterialWidget(QWidget *parent)
    : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_materialGroup = new QGroupBox(tr("Material"), this);
    auto *materialForm = new QFormLayout(m_materialGroup);
    m_intensity = addSliderRow(materialForm, tr("Overall intensity:"), 0, 200, tr("%"));
    m_refraction = addSliderRow(materialForm, tr("Refraction:"), 0, 128, tr(" px"));
    m_bezel = addSliderRow(materialForm, tr("Glass thickness:"), 0, 128, tr(" px"));
    m_magnify = addSliderRow(materialForm, tr("Magnification:"), 0, 30, tr("%"));
    m_dispersion = addSliderRow(materialForm, tr("Color fringing:"), 0, 100, tr("%"));
    m_frost = addSliderRow(materialForm, tr("Frost:"), 0, 100, tr("%"));
    m_saturation = addSliderRow(materialForm, tr("Saturation:"), 0, 300, tr("%"));
    m_brightness = addSliderRow(materialForm, tr("Brightness:"), 50, 200, tr("%"));

    m_tintButton = new QPushButton(m_materialGroup);
    m_tintButton->setMinimumWidth(84);
    connect(m_tintButton, &QPushButton::clicked, this, [this]() {
        const QColor chosen = QColorDialog::getColor(m_tintColor, this, tr("Glass Tint Color"));
        if (chosen.isValid() && chosen != m_tintColor) {
            m_tintColor = chosen;
            updateTintButton();
            emitIfInteractive();
        }
    });
    materialForm->addRow(tr("Tint:"), m_tintButton);

    m_tintStrength = addSliderRow(materialForm, tr("Tint strength:"), 0, 100, tr("%"));
    m_highlight = addSliderRow(materialForm, tr("Edge highlight:"), 0, 200, tr("%"));
    m_edgeShade = addSliderRow(materialForm, tr("Edge shading:"), 0, 100, tr("%"));
    m_sheen = addSliderRow(materialForm, tr("Sheen:"), 0, 100, tr("%"));
    layout->addWidget(m_materialGroup);

    m_pointerGroup = new QGroupBox(tr("Pointer"), this);
    auto *pointerForm = new QFormLayout(m_pointerGroup);
    m_pointerEffects = new QCheckBox(tr("Light and lens follow the pointer"), m_pointerGroup);
    connect(m_pointerEffects, &QCheckBox::toggled, this, [this](bool checked) {
        m_pointerBulge.slider->setEnabled(checked);
        m_pointerBulge.readout->setEnabled(checked);
        emitIfInteractive();
    });
    pointerForm->addRow(m_pointerEffects);
    m_pointerBulge = addSliderRow(pointerForm, tr("Lens bulge:"), 0, 32, tr(" px"));
    layout->addWidget(m_pointerGroup);

    setParameters(GlassParameters::globalFallback());
}

GlassMaterialWidget::~GlassMaterialWidget() = default;

GlassMaterialWidget::SliderRow GlassMaterialWidget::addSliderRow(
    QFormLayout *form, const QString &label, int min, int max, const QString &suffix) {
    auto *rowWidget = new QWidget(form->parentWidget());
    auto *rowLayout = new QHBoxLayout(rowWidget);
    rowLayout->setContentsMargins(0, 0, 0, 0);

    auto *slider = new QSlider(Qt::Horizontal, rowWidget);
    slider->setRange(min, max);
    auto *readout = new QLabel(rowWidget);
    readout->setMinimumWidth(48);
    readout->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto updateText = [readout, suffix](int value) {
        readout->setText(QString::number(value) + suffix);
    };
    updateText(slider->value());

    connect(slider, &QSlider::valueChanged, this, [this, updateText](int value) {
        updateText(value);
        emitIfInteractive();
    });

    rowLayout->addWidget(slider, 1);
    rowLayout->addWidget(readout);
    form->addRow(label, rowWidget);
    return {.slider = slider, .readout = readout};
}

void GlassMaterialWidget::setParameters(const GlassParameters &parameters) {
    m_updating = true;
    m_base = parameters;

    m_intensity.slider->setValue(std::clamp(pct(parameters.intensity), 0, 200));
    m_refraction.slider->setValue(std::clamp(px(parameters.refraction), 0, 128));
    m_bezel.slider->setValue(std::clamp(px(parameters.bezel), 0, 128));
    m_magnify.slider->setValue(std::clamp(pct(parameters.magnify), 0, 30));
    m_dispersion.slider->setValue(std::clamp(pct(parameters.dispersion), 0, 100));
    m_frost.slider->setValue(std::clamp(pct(parameters.frost), 0, 100));
    m_saturation.slider->setValue(std::clamp(pct(parameters.saturation), 0, 300));
    m_brightness.slider->setValue(std::clamp(pct(parameters.brightness), 50, 200));

    m_tintColor = parameters.tint;
    m_tintColor.setAlphaF(1.0F);
    updateTintButton();
    m_tintStrength.slider->setValue(std::clamp(pct(parameters.tint.alphaF()), 0, 100));

    m_highlight.slider->setValue(std::clamp(pct(parameters.highlight), 0, 200));
    m_edgeShade.slider->setValue(std::clamp(pct(parameters.edgeShade), 0, 100));
    m_sheen.slider->setValue(std::clamp(pct(parameters.sheen), 0, 100));

    const bool pointerOn = parameters.pointerLight > 0.001 || parameters.pointerBulge > 0.001;
    m_pointerEffects->setChecked(pointerOn);
    m_pointerBulge.slider->setEnabled(pointerOn);
    m_pointerBulge.readout->setEnabled(pointerOn);
    m_pointerBulge.slider->setValue(std::clamp(px(parameters.pointerBulge), 0, 32));

    m_updating = false;
}

GlassParameters GlassMaterialWidget::parameters() const {
    GlassParameters m = m_base;
    m.intensity = m_intensity.slider->value() / 100.0;
    m.refraction = m_refraction.slider->value();
    m.bezel = m_bezel.slider->value();
    m.magnify = m_magnify.slider->value() / 100.0;
    m.dispersion = m_dispersion.slider->value() / 100.0;
    m.frost = m_frost.slider->value() / 100.0;
    m.saturation = m_saturation.slider->value() / 100.0;
    m.brightness = m_brightness.slider->value() / 100.0;
    QColor tint = m_tintColor;
    tint.setAlphaF(static_cast<float>(m_tintStrength.slider->value() / 100.0));
    m.tint = tint;
    m.highlight = m_highlight.slider->value() / 100.0;
    m.edgeShade = m_edgeShade.slider->value() / 100.0;
    m.sheen = m_sheen.slider->value() / 100.0;
    const bool pointerOn = m_pointerEffects->isChecked();
    m.pointerLight = pointerOn ? (m_base.pointerLight > 0.001 ? m_base.pointerLight : 0.6) : 0.0;
    m.pointerBulge = pointerOn ? m_pointerBulge.slider->value() : 0.0;
    return m;
}

void GlassMaterialWidget::setMaterialGroupTitle(const QString &title) {
    m_materialGroup->setTitle(title);
}

void GlassMaterialWidget::setPointerGroupTitle(const QString &title) {
    m_pointerGroup->setTitle(title);
}

void GlassMaterialWidget::updateTintButton() {
    const double luma = 0.2126 * m_tintColor.redF() + 0.7152 * m_tintColor.greenF() + 0.0722 * m_tintColor.blueF();
    const QString textColor = luma > 0.5 ? QStringLiteral("#111111") : QStringLiteral("#ffffff");
    m_tintButton->setText(m_tintColor.name(QColor::HexRgb).toUpper());
    m_tintButton->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: %1; color: %2; border: 1px solid palette(mid); "
        "border-radius: 4px; padding: 4px 10px; }")
        .arg(m_tintColor.name(QColor::HexRgb), textColor));
}

void GlassMaterialWidget::emitIfInteractive() {
    if (!m_updating) {
        Q_EMIT parametersChanged(parameters());
    }
}

} // namespace fluctus::glass
