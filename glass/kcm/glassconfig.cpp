// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: GPL-3.0-or-later

#include "glassconfig.hpp"

#include "fluctusglassconfig.h"

#include <KColorButton>
#include <KLocalizedString>
#include <KPluginFactory>

#include <QCheckBox>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

namespace fluctus::glass {

namespace {

QCheckBox *checkBox(const char *name, const QString &text, QWidget *parent) {
    auto *box = new QCheckBox(text, parent);
    box->setObjectName(QLatin1String(name));
    return box;
}

QSlider *slider(const char *name, int min, int max, QWidget *parent) {
    auto *s = new QSlider(Qt::Horizontal, parent);
    s->setObjectName(QLatin1String(name));
    s->setRange(min, max);
    return s;
}

QSpinBox *radiusBox(const char *name, QWidget *parent) {
    auto *box = new QSpinBox(parent);
    box->setObjectName(QLatin1String(name));
    box->setRange(-1, 256);
    box->setSpecialValueText(i18nc("@item:inrange corner radius", "Automatic"));
    box->setSuffix(i18nc("@item:valuesuffix pixels", " px"));
    return box;
}

/// KConfigXT string lists edit as comma-separated text in a QLineEdit named kcfg_*.
QLineEdit *classList(const char *name, QWidget *parent) {
    auto *edit = new QLineEdit(parent);
    edit->setObjectName(QLatin1String(name));
    edit->setPlaceholderText(i18nc("@info:placeholder", "Window classes, comma separated"));
    return edit;
}

} // namespace

GlassEffectConfig::GlassEffectConfig(QObject *parent, const KPluginMetaData &data)
    : KCModule(parent, data) {
    FluctusGlassConfig::instance(QStringLiteral("kwinrc"));
    QWidget *root = widget();
    auto *layout = new QVBoxLayout(root);

    auto *intro = new QLabel(i18n("Apps such as Fluctus Dock request Liquid Glass for their own surfaces; "
                                  "everything else keeps the normal Blur effect. Turn on global mode to give every "
                                  "blurred surface Liquid Glass instead."), root);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto *global = new QGroupBox(i18nc("@title:group", "Global mode"), root);
    auto *globalForm = new QFormLayout(global);
    globalForm->addRow(checkBox("kcfg_GlobalMode", i18n("Use Liquid Glass in place of blur everywhere"), global));
    globalForm->addRow(checkBox("kcfg_GlobalWindows", i18n("Application windows"), global));
    globalForm->addRow(checkBox("kcfg_GlobalDecorations", i18n("Window title bars"), global));
    globalForm->addRow(checkBox("kcfg_GlobalMenus", i18n("Menus, popups and tooltips"), global));
    globalForm->addRow(checkBox("kcfg_GlobalPanels", i18n("Panels and docks"), global));
    globalForm->addRow(checkBox("kcfg_GlobalNotifications", i18n("Notifications and on-screen displays"), global));
    globalForm->addRow(i18n("Never use glass for:"), classList("kcfg_ExcludedClasses", global));
    globalForm->addRow(i18n("Always use glass for:"), classList("kcfg_ForcedClasses", global));
    layout->addWidget(global);

    auto *shape = new QGroupBox(i18nc("@title:group", "Corners (global mode)"), root);
    auto *shapeForm = new QFormLayout(shape);
    shapeForm->addRow(i18n("Windows:"), radiusBox("kcfg_WindowRadius", shape));
    shapeForm->addRow(i18n("Menus and popups:"), radiusBox("kcfg_MenuRadius", shape));
    shapeForm->addRow(i18n("Panels:"), radiusBox("kcfg_PanelRadius", shape));
    layout->addWidget(shape);

    auto *material = new QGroupBox(i18nc("@title:group", "Material (global mode)"), root);
    auto *materialForm = new QFormLayout(material);
    materialForm->addRow(i18n("Overall intensity:"), slider("kcfg_Intensity", 0, 200, material));
    materialForm->addRow(i18n("Refraction:"), slider("kcfg_Refraction", 0, 128, material));
    materialForm->addRow(i18n("Glass thickness:"), slider("kcfg_Bezel", 0, 128, material));
    materialForm->addRow(i18n("Magnification:"), slider("kcfg_Magnify", 0, 30, material));
    materialForm->addRow(i18n("Color fringing:"), slider("kcfg_Dispersion", 0, 100, material));
    materialForm->addRow(i18n("Frost:"), slider("kcfg_Frost", 0, 100, material));
    materialForm->addRow(i18n("Saturation:"), slider("kcfg_Saturation", 0, 300, material));
    materialForm->addRow(i18n("Brightness:"), slider("kcfg_Brightness", 50, 200, material));
    auto *tint = new KColorButton(material);
    tint->setObjectName(QStringLiteral("kcfg_TintColor"));
    materialForm->addRow(i18n("Tint:"), tint);
    materialForm->addRow(i18n("Tint strength:"), slider("kcfg_TintStrength", 0, 100, material));
    materialForm->addRow(i18n("Edge highlight:"), slider("kcfg_Highlight", 0, 200, material));
    materialForm->addRow(i18n("Edge shading:"), slider("kcfg_EdgeShade", 0, 100, material));
    materialForm->addRow(i18n("Sheen:"), slider("kcfg_Sheen", 0, 100, material));
    layout->addWidget(material);

    auto *pointer = new QGroupBox(i18nc("@title:group", "Pointer"), root);
    auto *pointerForm = new QFormLayout(pointer);
    pointerForm->addRow(checkBox("kcfg_PointerEffects", i18n("Light and lens follow the pointer"), pointer));
    pointerForm->addRow(i18n("Lens bulge:"), slider("kcfg_PointerBulge", 0, 32, pointer));
    layout->addWidget(pointer);
    layout->addStretch(1);

    addConfig(FluctusGlassConfig::self(), root);
}

void GlassEffectConfig::save() {
    KCModule::save();
    QDBusMessage message = QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.KWin"),
        QStringLiteral("/Effects"),
        QStringLiteral("org.kde.kwin.Effects"),
        QStringLiteral("reconfigureEffect"));
    message << QStringLiteral("fluctusglass");
    QDBusConnection::sessionBus().asyncCall(message);
}

} // namespace fluctus::glass

using fluctus::glass::GlassEffectConfig;
K_PLUGIN_CLASS(GlassEffectConfig)

#include "glassconfig.moc"
