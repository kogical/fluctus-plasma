// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: GPL-3.0-or-later

#include "glassconfig.hpp"

#include "fluctusglassconfig.h"

#include "FluctusGlass/GlassMaterialWidget.hpp"

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
#include <QSpinBox>
#include <QVBoxLayout>

#include <cmath>

namespace fluctus::glass {

namespace {

QCheckBox *checkBox(const char *name, const QString &text, QWidget *parent) {
    auto *box = new QCheckBox(text, parent);
    box->setObjectName(QLatin1String(name));
    return box;
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

GlassParameters materialFromConfig(const FluctusGlassConfig *cfg) {
    GlassParameters m = GlassParameters::globalFallback();
    m.intensity = cfg->intensity() / 100.0;
    m.refraction = cfg->refraction();
    m.bezel = cfg->bezel();
    m.magnify = cfg->magnify() / 100.0;
    m.dispersion = cfg->dispersion() / 100.0;
    m.frost = cfg->frost() / 100.0;
    m.saturation = cfg->saturation() / 100.0;
    m.brightness = cfg->brightness() / 100.0;
    QColor tint = cfg->tintColor();
    tint.setAlphaF(static_cast<float>(cfg->tintStrength() / 100.0));
    m.tint = tint;
    m.highlight = cfg->highlight() / 100.0;
    m.edgeShade = cfg->edgeShade() / 100.0;
    m.sheen = cfg->sheen() / 100.0;
    m.pointerLight = cfg->pointerEffects() ? 0.6 : 0.0;
    m.pointerBulge = cfg->pointerEffects() ? cfg->pointerBulge() : 0.0;
    return m;
}

void writeMaterialToConfig(const GlassParameters &m, FluctusGlassConfig *cfg) {
    cfg->setIntensity(static_cast<int>(std::lround(m.intensity * 100.0)));
    cfg->setRefraction(static_cast<int>(std::lround(m.refraction)));
    cfg->setBezel(static_cast<int>(std::lround(m.bezel)));
    cfg->setMagnify(static_cast<int>(std::lround(m.magnify * 100.0)));
    cfg->setDispersion(static_cast<int>(std::lround(m.dispersion * 100.0)));
    cfg->setFrost(static_cast<int>(std::lround(m.frost * 100.0)));
    cfg->setSaturation(static_cast<int>(std::lround(m.saturation * 100.0)));
    cfg->setBrightness(static_cast<int>(std::lround(m.brightness * 100.0)));
    QColor tint = m.tint;
    tint.setAlphaF(1.0F);
    cfg->setTintColor(tint);
    cfg->setTintStrength(static_cast<int>(std::lround(m.tint.alphaF() * 100.0)));
    cfg->setHighlight(static_cast<int>(std::lround(m.highlight * 100.0)));
    cfg->setEdgeShade(static_cast<int>(std::lround(m.edgeShade * 100.0)));
    cfg->setSheen(static_cast<int>(std::lround(m.sheen * 100.0)));
    const bool pointerOn = m.pointerLight > 0.001 || m.pointerBulge > 0.001;
    cfg->setPointerEffects(pointerOn);
    cfg->setPointerBulge(static_cast<int>(std::lround(m.pointerBulge)));
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

    m_materialEditor = new GlassMaterialWidget(root);
    m_materialEditor->setMaterialGroupTitle(i18nc("@title:group", "Material (global mode)"));
    m_materialEditor->setPointerGroupTitle(i18nc("@title:group", "Pointer"));
    connect(m_materialEditor, &GlassMaterialWidget::parametersChanged, this, [this]() {
        setNeedsSave(true);
        setRepresentsDefaults(false);
    });
    layout->addWidget(m_materialEditor);
    layout->addStretch(1);

    addConfig(FluctusGlassConfig::self(), root);
}

void GlassEffectConfig::load() {
    KCModule::load();
    FluctusGlassConfig::self()->read();
    m_materialEditor->setParameters(materialFromConfig(FluctusGlassConfig::self()));
}

void GlassEffectConfig::defaults() {
    KCModule::defaults();
    m_materialEditor->setParameters(GlassParameters::globalFallback());
    setNeedsSave(true);
}

void GlassEffectConfig::save() {
    KCModule::save();
    writeMaterialToConfig(m_materialEditor->parameters(), FluctusGlassConfig::self());
    FluctusGlassConfig::self()->save();
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
