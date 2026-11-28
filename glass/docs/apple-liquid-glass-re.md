# Apple Liquid Glass (iOS 26 / macOS 26 Tahoe) — Reverse-Engineering Reference

This document compiles **verified** technical findings on Apple's **Liquid Glass** implementation in iOS 26 and macOS 26 (Tahoe), strictly separating binary/runtime dumps from WWDC25 documentation, empirical observations, and unverified/unknown shader internals.

---

## 1. Verified from Apple Binaries, Runtime Dumps & Private API Introspection

### 1.1 QuartzCore / SwiftUI / UIKit Layer Hierarchy
Reconstructed from macOS 26 runtime inspection in [`AlexStrNik/ShatteredGlass`](https://github.com/AlexStrNik/ShatteredGlass/blob/main/ShatteredGlass/CustomGlassView.swift) (`CustomGlassView.swift`, `ShatteredGlass-Bridging-Header.h`) and UIKit/SwiftUI hierarchy analyses ([Fatbobman Issue #88 / Cyandev](https://fatbobman.com/), [Jacob's Tech Tavern — *Forbidden iOS*](https://jacobstechtavern.com/)):

```text
SwiftUI.SDFLayer (CALayer subclass; UIKit uses _UIMultiLayer)
├── CABackdropLayer (name: "@0")
│   │   scale = 0.25, tracksLuma = true, allowsFilteredLuma = true,
│   │   windowServerAware = true, allowsGroupOpacity = false
│   │   filters = [CAFilter(type: "glassBackground")]
│   └── CASDFLayer (name: "@0", smoothness = 8)
│       │   effect = CASDFOutputEffect(minimum: -1000, maximum: 1)
│       └── CALayer (container)
│           └── CASDFElementLayer
│                   cornerRadius = size.width / 2,
│                   gradientOvalization = 0.5, mode = "bounds", operation = "union"
├── CASDFLayer (name: "@1", smoothness = 8) — Highlight Pass 1 (+45°)
│   │   effect = CASDFGlassHighlightEffect(angle: +0.78539816339744828, ...)
│   │   filters = [CAFilter(type: "vibrantColorMatrix")]
│   └── SDFPortalLayer (_TtC7SwiftUIP33_05C1F5BE6EE7940FED05100EA822F7B314SDFPortalLayer)
│           -> sources CASDFElementLayer
└── CASDFLayer (name: "@2", smoothness = 8) — Highlight Pass 2 (-135°)
    │   effect = CASDFGlassHighlightEffect(angle: -2.3561944901923448, ...)
    │   filters = [CAFilter(type: "vibrantColorMatrix")]
    └── SDFPortalLayer
            -> sources CASDFElementLayer
```

Key architectural facts:
- **SDF-driven pipeline:** Geometry is defined once in `CASDFElementLayer` (`mode = "bounds"`, `operation = "union"`, `gradientOvalization = 0.5`). `CASDFLayer` (`smoothness = 8`) rasterizes/evaluates the SDF with `CASDFOutputEffect` (`minimum = -1000`, `maximum = 1`) into a texture named `"@0"` consumed by `glassBackground`.
- **Two opposing highlight layers (`@1`, `@2`):** Specular rim highlights are **not** part of `glassBackground`; they are rendered as two separate `CASDFLayer` siblings using `SDFPortalLayer` (a `CAPortalLayer` subclass) to share `CASDFElementLayer` without duplicating geometry.
- **Backdrop downscaling:** `CABackdropLayer.setScale(0.25)` captures the underlying backdrop at 1/4 resolution before filtering. `tracksLuma = true` and `allowsFilteredLuma = true` drive adaptive brightness tracking.
- **Other private `QuartzCore` SDF classes** observed in iOS/macOS runtime headers: `CASDFGlassDisplacementEffect`, `CASDFShadowEffect`, `CASDFGradientEffect`, `CASDFGradientContourEffect`, `CASDFFillEffect`, `CASDFVisualizationEffect`.
- **AppKit `NSGlassEffectView` private selectors** verified in [`vientooscuro/WholeWindowLiquidGlass`](https://github.com/vientooscuro/WholeWindowLiquidGlass/blob/main/Sources/WholeWindowLiquidGlass/WholeWindowLiquidGlass.swift) and Habr ([Daniil Nuzhdin](https://habr.com/)):
  - `set_variant:` (passing `6` selects the light HUD pill glass; full-window `.clear` leaves it `nil`).
  - `set_scrimState:`, `set_subduedState:` (discovered in binary dumps; exact enum values undocumented).
  - SwiftUI `.glassEffect` cannot sample behind its own window; only `NSGlassEffectView` samples the desktop backdrop via WindowServer.

---

### 1.2 `CAFilter("glassBackground")` — Exact Keys & macOS 26 Dumped Values
All 49 input keys verified in [`quentinfasquel/CAFilterBuiltins`](https://github.com/quentinfasquel/CAFilterBuiltins/blob/main/Sources/CAFilterBuiltins/CAFilterBuiltins.swift) (`CAGlassBackground` protocol) and [`AlexStrNik/ShatteredGlass`](https://github.com/AlexStrNik/ShatteredGlass/blob/main/ShatteredGlass/CustomGlassView.swift):

| Group | Key | Dumped Value (`ShatteredGlass`) | Notes / Role |
|---|---|---|---|
| **Source** | `inputSourceSublayerName` | `"@0"` | Links to the `CASDFLayer` running `CASDFOutputEffect` |
| **Refraction** | `inputInnerRefractionAmount` | `-40` | Negative (inward/concave or inverted sign relative to outer) |
| | `inputInnerRefractionHeight` | `12.5` | Inner lens height/thickness |
| | `inputOuterRefractionAmount` | `10` | Positive (outward/convex rim bending) |
| | `inputOuterRefractionHeight` | `6.25` | Outer rim height/thickness |
| | `inputRefractionDistance0` | `-1` | SDF distance band 0 |
| | `inputRefractionDistance1` | `-0.5` | SDF distance band 1 |
| | `inputRefractionOpacity` | `1` | Overall refraction blend weight |
| **Progressive Blur** | `inputBlurRadius` | `1.5` (`~7–8` stock `.clear`, `~2.5` HUD) | Base blur radius (`WholeWindowLiquidGlass`) |
| | `inputBlurDistance0..4` | `[-25, -1, 0, 0, 10]` | 5 spatial SDF distance bands |
| | `inputBlurOpacity0..4` | `[1, 0.5, 0.5, 1, 1]` | 5 band opacities; zeroing these removes the "milky mush" (`WholeWindowLiquidGlass`) |
| **Face (Interior)** | `inputFaceOpacity` | `1` | Interior body opacity |
| | `inputFaceColorMatrixWhite` | `0.3` | White point lift |
| | `inputFaceColorMatrixBlack` | `0.1` | Black point lift |
| | `inputFaceColorMatrixSaturation` | `1` | Saturation multiplier |
| | `inputFaceColorMatrixFillColor` | `sRGB(1, 1, 1, 0.02)` | Tint fill color |
| | `inputFaceColorMatrixMaxLuma` / `MaxLumaSDR` | *(present in `WholeWindowLiquidGlass`)* | Clamps interior luminance |
| **Bleed (Color Spill)** | `inputBleedAmount` | `17.5` | Neighboring color bleed magnitude |
| | `inputBleedHeight` | `17.5` | Bleed height |
| | `inputBleedBlurRadius` | `35` | Wide blur for ambient color spill |
| | `inputBleedDistance0`, `1` | `1`, `0` | SDF distance range for bleed |
| | `inputBleedOpacity` | `0.01` | Subtle bleed weight |
| | `inputBleedDarkenBlend` | `0` (`Bool`) | Darken blend toggle |
| | `inputBleedColorMatrixBlack/White/Sat/Fill` | `0`, `0.5`, `1`, `nil` | Bleed color transform |
| **Shadow** | `inputShadowOffset` | `(0, 8)` | Y-downward drop shadow offset |
| | `inputShadowAmount` | `31.25` | Shadow strength |
| | `inputShadowHeight` | `20` | Shadow height |
| | `inputShadowDistanceOffset` | `0` | SDF distance offset |
| | `inputShadowBlurRadius` | `40` | Shadow blur radius |
| | `inputShadowRadius` | `24` | Shadow spread/radius |
| | `inputShadowOpacity` | `0.4` | Shadow opacity |
| | `inputShadowVibrancyContribution` | `0.01` | Vibrancy bleed into shadow |
| | `inputShadowColorMatrixBlack/White/Sat/Fill` | `0`, `0.5`, `1`, `nil` | Shadow color matrix |
| **SDR / HDR** | `inputSDRHoldingToneEnabled` | `1` (`Bool`) | Tone-mapping guard for SDR |
| | `inputSDRHoldingToneWhite` | *(in `CAGlassBackground`)* | SDR holding white |
| | `inputSDRGradientDistance0`, `1` | `-1`, `-0.5` | SDR gradient SDF bounds |
| | `inputSDRShadowOpacity` | `0.12` | SDR shadow opacity |
| | `inputMaxHeadroom` | `9999` | HDR headroom |
| | `inputClamp`, `inputClampPreserveHue` | *(in `CAGlassBackground`)* | Output clamping |

---

### 1.3 `CAFilter("glassForeground")` & Chromatic Aberration Filters
Verified in [`CAFilterBuiltins.swift`](https://github.com/quentinfasquel/CAFilterBuiltins/blob/main/Sources/CAFilterBuiltins/CAFilterBuiltins.swift) (`CAFilterType.swift` + protocols):
- **Chromatic dispersion/aberration lives in `glassForeground` (and standalone `chromaticAberration` / `chromaticAberrationMap` filters), NOT in `glassBackground`.**
- `CAGlassForeground` keys:
  - `inputSourceSublayerName`
  - `inputRefractionAmount`, `inputRefractionHeight`, `inputRefractionOffset`
  - `inputAberrationAmount`, `inputAberrationHeight`, `inputAberrationOffset`, `inputAberrationAngle`
  - `inputEdgeStart`, `inputEdgeEnd`, `inputEdgeOpacityStart`, `inputEdgeOpacityEnd`
- `CAChromaticAberration` keys: `inputRedOffset` (`CGPoint`), `inputGreenOffset` (`CGPoint`), `inputBlueOffset` (`CGPoint`), `inputExtendEdges` (`Bool`).
- `CAChromaticAberrationMap` keys: `inputMaskImage` (`CGImage`), `inputOffset` (`CGPoint`), `inputAmount` (`CGFloat`), `inputSourceSublayerName`.

---

### 1.4 `CASDFGlassHighlightEffect` & `vibrantColorMatrix` (Rim Specular)
Verified in [`ShatteredGlass/CustomGlassView.swift`](https://github.com/AlexStrNik/ShatteredGlass/blob/main/ShatteredGlass/CustomGlassView.swift) and `ShatteredGlass-Bridging-Header.h`:
- `CASDFGlassHighlightEffect` properties:
  - `amount`: `0.5`
  - `angle`: **Dual opposing corners** — Pass 1 (`@1`) at `+0.78539816339744828` rad ($+\pi/4 = +45^\circ$), Pass 2 (`@2`) at `-2.3561944901923448` rad ($-3\pi/4 = -135^\circ$).
  - `color`: `sRGB(1, 1, 1, 1)`
  - `curvature`: `1.7`
  - `height`: `1`
  - `spread`: `1.59`
  - `global`: `bool`
- Paired with `CAFilter(type: "vibrantColorMatrix")` using a 4×5 `CAColorMatrix`:
  ```text
  [  2.6705   -1.1088   -0.1117   0.0   0.05 ]
  [ -0.3295    1.8914   -0.1119   0.0   0.05 ]
  [ -0.3297   -1.1084    2.8881   0.0   0.05 ]
  [  0.0       0.0       0.0      1.0   0.0  ]
  ```
  (High-diagonal saturation boost + negative cross-channel terms + `+0.05` additive RGB bias.)

---

### 1.5 Metal Shader Library & Corner Geometry
- **`QuartzCore.framework/default.metallib`**: Binary inspection confirms compiled AIR shader functions including `glass_background_sdf_all_lpf` and `glass_background_all_lpf` (SDF evaluation + low-pass filtering).
- **Corner curves (`CALayerCornerCurve.continuous`)**: Core Animation approximates $G^2$-continuous corners ("squircles") using hardware-accelerated cubic Bézier segments rather than evaluating exact Lamé superellipses ($|x|^n + |y|^n = r^n$) per frame.

---

## 2. Verified from Apple WWDC25 Sessions & Official Documentation
Sources: WWDC25 Session 219 (*"Meet Liquid Glass"*), Session 323 (*"Build a SwiftUI app with the new design"*), Apple Developer Documentation (`Glass`, `.glassEffect(_:in:)`, `GlassEffectContainer`), Apple Newsroom (June 9, 2025).

- **Lensing vs. scattering:** Unlike legacy `Material` blurs that scatter light uniformly, Liquid Glass bends/refracts underlying content along edges (**lensing**) and reflects surrounding light.
- **Variants:**
  - `Glass.regular`: Default adaptive variant; higher diffusion and face opacity for general navigation/chrome; adapts dynamically between light and dark depending on backdrop luminance.
  - `Glass.clear`: High-translucency variant with minimal diffusion for media-rich backgrounds; often paired with a subtle dimming scrim for legibility.
- **Interactive response:** `.interactive(true)` energizes highlights and flexes/scales geometry on pointer/touch interaction.
- **Morphing & Concentricity:** `GlassEffectContainer(spacing:)` merges adjacent glass shapes smoothly (consistent with `CASDFElementLayer` `operation = "union"` and `CASDFLayer` `smoothness = 8`). Nested shapes use concentric corner radii (`ConcentricRectangle` / `.continuous`).
- **Color spill & adaptive shadows:** Neighboring bright colors subtly bleed onto the glass surface, and background shadows deepen when high-contrast text scrolls underneath.

---

## 3. Direct Visual / Empirical Observations on Real OS Captures
- **Two-zone refraction profile:** Real macOS 26 / iOS 26 captures show a largely undistorted/lightly blurred interior (`inputBlurRadius` + `inputFaceColorMatrix*`) surrounded by a narrow rim band where background lines bend sharply (`inputOuterRefractionAmount: 10`, `inputInnerRefractionAmount: -40`, `inputRefractionDistance0..1: [-1, -0.5]`).
- **Progressive blur causes the "milky" look:** Verified empirically by `WholeWindowLiquidGlass` on macOS 26 — stock `.clear` uses `inputBlurRadius ~7–8` plus non-zero `inputBlurOpacity0..4`; zeroing `inputBlurOpacity0..4` and lowering `inputBlurRadius` to `2.5` reproduces the crisp system volume/brightness HUD look.
- **Dual-corner specular rim:** Visible as bright opposing corner arcs (top-left/bottom-right at $45^\circ$ / $-135^\circ$) that intensify over saturated backdrops due to `vibrantColorMatrix`.

---

## 4. Explicitly Unverified / Unknown (Do Not Assume)
No public source as of this report has published decompiled Metal source from `QuartzCore.default.metallib` or an annotated Xcode GPU Frame Capture of `backboardd`/`WindowServer` for Liquid Glass. Specifically **unverified**:
1. **Exact displacement math inside `glassBackground`:** Whether inner/outer refraction computes a 3D heightmap normal from `inputInnerRefractionHeight`/`inputOuterRefractionHeight` + Snell's law (and what IOR), or a direct SDF-gradient offset curve.
2. **Exact dispersion implementation in `glassForeground`:** Exact RGB wavelength offsets, tap count, and whether dispersion is radial along the SDF gradient or directional along `inputAberrationAngle`.
3. **Exact blur kernel:** Whether the 5 progressive blur bands (`inputBlurDistance0..4`, `inputBlurOpacity0..4`) use Dual-Kawase mip-chain sampling, separable Gaussian, or a custom low-pass filter (`*_lpf`).
4. **Exact highlight blending & Fresnel curve in `CASDFGlassHighlightEffect`:** Whether the output composites with `PlusLighter` (additive), `screen`, or `sourceOver`, and the exact falloff function mapping `curvature` (`1.7`), `spread` (`1.59`), `height` (`1`), and `angle`.
5. **Complete `set_variant:` table** for `NSGlassEffectView` / `UIGlassEffect` (only `6` = light HUD pill is verified).

---

## 5. Verified High-Resolution Apple Newsroom Reference Images
All URLs below are direct `large_2x.jpg` assets from Apple's June 9, 2025 WWDC25 Newsroom announcements:

- **macOS Tahoe 26 Control Center** (glass tiles, sliders, pills over wallpaper):
  `https://www.apple.com/newsroom/images/2025/06/macos-tahoe-26-makes-the-mac-more-capable-productive-and-intelligent-than-ever/article/Apple-WWDC25-macOS-Tahoe-26-Control-Center-250609_big.jpg.large_2x.jpg`
- **macOS Tahoe 26 Lock Screen** (large glass clock & controls):
  `https://www.apple.com/newsroom/images/2025/06/macos-tahoe-26-makes-the-mac-more-capable-productive-and-intelligent-than-ever/article/Apple-WWDC25-macOS-Tahoe-26-Lock-Screen-250609_big.jpg.large_2x.jpg`
- **macOS Tahoe 26 Hero** (desktop with translucent menu bar, Dock, Control Center):
  `https://www.apple.com/newsroom/images/2025/06/macos-tahoe-26-makes-the-mac-more-capable-productive-and-intelligent-than-ever/article/Apple-WWDC25-macOS-Tahoe-26-hero-250609_big.jpg.large_2x.jpg`
- **Liquid Glass Hero** (clean showcase of glass pills/icons/refractions):
  `https://www.apple.com/newsroom/images/2025/06/apple-introduces-a-delightful-and-elegant-new-software-design/article/Apple-WWDC25-Liquid-Glass-hero-250609_big.jpg.large_2x.jpg`
- **iOS 26 Home Screen — Clear Look** (clear glass icons/widgets over wallpaper):
  `https://www.apple.com/newsroom/images/2025/06/apple-introduces-a-delightful-and-elegant-new-software-design/article/Apple-WWDC25-Liquid-Glass-Home-Screen-clear-look-250609_big.jpg.large_2x.jpg`
- **iOS 26 Home Screen — Dark Tint** (tinted dark glass over wallpaper):
  `https://www.apple.com/newsroom/images/2025/06/apple-introduces-a-delightful-and-elegant-new-software-design/article/Apple-WWDC25-Liquid-Glass-Home-Screen-dark-tint-250609_big.jpg.large_2x.jpg`
- **iOS 26 Hero** (Lock Screen glass clock & bottom pill controls):
  `https://www.apple.com/newsroom/images/2025/06/apple-elevates-the-iphone-experience-with-ios-26/article/Apple-WWDC25-iOS-26-hero-250609_big.jpg.large_2x.jpg`
