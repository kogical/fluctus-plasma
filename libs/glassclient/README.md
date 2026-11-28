# Fluctus Glass client

A small Apache-2.0 library that lets any Qt app ask [Fluctus Glass](../../glass/README.md) for
Liquid Glass behind parts of its own windows. It only talks D-Bus to the effect and never links
GPL code, so you can embed it anywhere.

It comes in three pieces:

| | For | Installed |
| --- | --- | --- |
| `Kogical.FluctusGlass` | QML: `GlassArea`, `GlassProfile`, and `GlassSettingsDialog` | with Fluctus Glass, on the system QML path |
| `Fluctus::GlassClient` | C++: `GlassRequest`, `GlassProfile`, `GlassProtocol.hpp` | static library inside this repository |
| `Fluctus::GlassWidgets` | C++ (Qt Widgets): `GlassMaterialWidget`, `GlassSettingsDialog` | static library inside this repository (also embedded by the Desktop Effects KCM) |

## QML

```qml
import QtQuick
import Kogical.FluctusGlass

Window {
    color: "transparent"           // leave the area behind the glass see-through

    GlassArea {
        anchors.fill: parent
        anchors.margins: 16
        radius: 24
        profile: Plasmoid.configuration.glassProfile // optional: tracks a GlassProfile JSON string
    }
}
```

To let users customize Liquid Glass for your app or plasmoid only (with **Global Default**, **Light**, **Medium**, **Full**, **Crystal**, **Plain Blur**, **Off**, **Reset to Global Defaults**, and live sliders):

```qml
import QtQuick.Controls as QQC2
import Kogical.FluctusGlass

QQC2.Button {
    text: qsTr("Customize Liquid Glass…")
    onClicked: glassDialog.open()
}

GlassSettingsDialog {
    id: glassDialog
    profile: page.cfg_glassProfile
    onProfileChanged: page.cfg_glassProfile = profile
}
```

## C++ Widgets

```cpp
#include <FluctusGlass/GlassSettingsDialog.hpp>

if (const auto updated = fluctus::glass::GlassSettingsDialog::editProfile(savedJson, this)) {
    savedJson = *updated;
}
```

`GlassArea` draws nothing itself. It tells the effect where your glass is and keeps that in
sync as the item moves or resizes. Useful properties:

| Property | Meaning |
| --- | --- |
| `glassEnabled` | Turn the request on or off without removing the item |
| `mode` | `"glass"` (default), `"plain"` (frosted blur, even in global mode) or `"none"` (no backdrop effect) |
| `blurOnly` | Only set a rounded blur region; becomes glass when the user runs global mode |
| `intensity` | Master strength, 0–2 (default 1) |
| `radius`, `tint`, `bezel`, `refraction`, `dispersion`, `frost`, `saturation`, `brightness` | The material |
| `highlight`, `highlightWidth`, `sheen`, `lightAngle`, `edgeShade`, `grain` | Lighting and finish |
| `pointerLight`, `pointerBulge`, `pointerRadius` | How the glass reacts to the pointer |
| `available`, `active` | Read-only: the effect is running / it is rendering this area |

See [GlassArea.hpp](qml/GlassArea.hpp) for the complete list, and
[glass/examples](../../glass/examples/) for runnable demos.

## C++

```cpp
#include <FluctusGlass/GlassRequest.hpp>

auto *glass = new fluctus::glass::GlassRequest(this);
glass->setWindow(window);
glass->setParameters(fluctus::glass::GlassParameters::preset(u"crystal"));
glass->setRegion(QRectF(16, 16, 400, 64), 24);
```

`setRegion` also sets the matching KWin blur region. That region gives the glass its shape, and
it is what users without Fluctus Glass see: plain blur, so your app degrades gracefully.

## The protocol

The D-Bus contract (service `com.kogical.Fluctus.Glass`, interface `com.kogical.Fluctus.Glass1`)
is documented in [GlassProtocol.hpp](include/FluctusGlass/GlassProtocol.hpp). An app can only
ask for glass on its own windows: the effect checks the caller's process id.
