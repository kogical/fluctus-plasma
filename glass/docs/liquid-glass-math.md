# Liquid Glass — Math, Optics and Shader Specification

Target: Fluctus Glass compositor shader (`glass/src/shaders/glass.frag`).
Scope: Apple macOS 26 (Tahoe) / iOS 26 "Liquid Glass".
Researched: 2026-11-28.

---

## 0. Read this first: what is and is not public

Apple does **not** publish Liquid Glass equations, IOR values, bezel profiles, blur radii, or shader code. The only public Apple sources (WWDC25 session 219 "Meet Liquid Glass", the Human Interface Guidelines Materials page, and the Liquid Glass technology overview) describe behaviour in prose. The single numeric Apple value is the **35 % dark dimming layer** for clear glass over bright content.

Everything below is tagged with its provenance:

| Tag | Meaning |
|-----|---------|
| **[D]** | Documented by Apple (WWDC25 / HIG), quoted or paraphrased. |
| **[C]** | Community or third-party replication that publishes concrete numbers or code (kube.io, Haze, Linear). These are *approximations of* Apple's look, not Apple's values. |
| **[P]** | Physics or standard optics (Snell, Schlick, Cauchy). Exact for the physics, but the *choice* of parameters is ours. |
| **[E]** | Engineering choice made in this spec. Starting value to tune against reference screenshots. |
| **[U]** | Unknown. No public source. Must be calibrated by eye against real macOS 26 screenshots. |

A 1:1 numerical match with Apple is **not achievable from public information**. The realistic target is: (a) the documented behaviour, (b) the physically-correct optical model driven by parameters tuned to side-by-side screenshots taken from the user's own machine.

**Version caveat.** Haze's Glass page says its built-in styles are "informed by iOS 27 Liquid Glass" [C]. Apple's look has been changing between releases, so tune against the macOS 26 target specifically.

---

## 1. Surface geometry and SDF

### 1.1 Continuous-curvature corners

[D] Apple describes `continuous` corners as curvature-continuous; SwiftUI's `.continuous` corner style is what Haze's `ContinuousRoundedCornerShape` defaults to (smoothing `1f`) [C].

[C] The standard model is the **superellipse / Lamé curve**:

```
|x/a|^n + |y/b|^n = 1            (n = 2 circle, n = 4 classic squircle, n ≈ 5 frequently cited for iOS)
```

- John D. Cook: "Some sources define the squircle to be the case p = 4 but others say p = 5." [C] https://www.johndcook.com/blog/2018/02/13/squircle-curvature/
- squircle.js: `cornerSmoothing` blends from a pure rounded rectangle (0) to a true squircle corner (1), default 0.6 for iOS-like icons. It states Apple's icon outline "is not a pure superellipse" and is built from tuned Bézier curves. No parameter-to-n mapping is published. [C] https://squircle.js.org/blog/math-behind-squircles
- Apple does not publish the exact curve. The "22.37 % of width, 60 % smoothing" figure is third-party and **unverified** [U].

[E] Proposed mapping for the shader: `n = mix(2.0, 4.0, smoothing)`, where `smoothing ∈ [0, 1]`. `smoothing = 0` gives a circle, `smoothing = 1` gives the classic squircle. Tune `n` to 5 if the corners look too flat.

### 1.2 SDF for a continuous rounded box (GLSL)

The Lamé corner is not an exact Euclidean SDF. It is accurate enough for shading and AA when used with `fwidth`.

```glsl
// p: point relative to box centre (px)
// b: half-size of the box (px)
// R: corner radius (px)
// n: superellipse exponent (2 = circle, 4 = squircle)
float continuousRoundedBoxSDF(vec2 p, vec2 b, float R, float n)
{
    vec2 q  = abs(p) - (b - R);        // offset from the corner centre
    vec2 qc = max(q, 0.0);
    if (q.x > 0.0 && q.y > 0.0) {
        // superellipse with semi-axes R, measured from the corner centre
        float s = pow(pow(qc.x / R, n) + pow(qc.y / R, n), 1.0 / n);
        return (s - 1.0) * R;
    }
    return max(q.x, q.y) - R;          // straight edges and interior strips
}

// Inward unit gradient (for surface normals and the rim).
vec2 sdfInwardDir(vec2 p, vec2 b, float R, float n)
{
    const float e = 0.5;               // px
    vec2 g = vec2(
        continuousRoundedBoxSDF(p + vec2(e, 0), b, R, n) - continuousRoundedBoxSDF(p - vec2(e, 0), b, R, n),
        continuousRoundedBoxSDF(p + vec2(0, e), b, R, n) - continuousRoundedBoxSDF(p - vec2(0, e), b, R, n));
    return -normalize(g + 1e-6);       // points inward (SDF grows outward)
}
```

Continuity check: at `q.x → 0+` the Lamé branch gives `(q.y/R - 1)·R = q.y − R`, which matches the strip branch. At `q.x = R, q.y = 0` it gives 0 (on the edge).

### 1.3 Smooth union (future, for merged glass)

[C] Haze's `Modifier.hazeGlassContainer` merges nearby surfaces into one shape [C]. The standard polynomial smooth-min is:

```glsl
float smin(float a, float b, float k)           // k = blend radius (px)
{
    float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
    return mix(b, a, h) - k * h * (1.0 - h);
}
```

Apply it to two SDFs before taking the normal and bezel. Not needed for the first pass.

---

## 2. Height field, bezel profile, and Snell refraction

### 2.1 Whole-body or edge-only lensing?

- [D] Apple: "The primary way Liquid Glass visually defines itself is through something called Lensing." [WWDC25 219, "Dynamics"]
- [D] Apple: Larger elements "have more pronounced lensing and refraction effects" [WWDC25 219, "Adaptivity"].
- [C] Haze Regular uses `RefractionProfile.Edge(20.dp)`. Refraction is greatest at the boundary and fades to none over that width. Clear uses `Edge(28.dp)`, with refraction "concentrated in a band along the edge that loops around corners" [C].
- [C] kube.io observes that Apple's glass "appears to favor convex profiles" and uses a lip bezel for its switch demo [C].
- [P] Physics: a flat slab at normal incidence has **zero** lateral displacement (proved in §2.4). The centre therefore stays crisp, and all visible lensing comes from the tilted bezel.

**Conclusion [E]:** edge-band lensing with a flat centre is the best-supported model. Haze's `Surface` profile (full-body dome) is an option, not the default.

### 2.2 Height profiles h(x)

[C] kube.io defines these with `x ∈ [0, 1]`, measured from the **outer edge** (0) to the end of the bezel (1). Sources: https://kube.io/blog/liquid-glass-css-svg/ (read in full via web-fetch this session).

| Profile | h(x) | h′(x) (analytic) |
|---------|------|------------------|
| Convex circle | `sqrt(1 − (1−x)²)` | `(1−x) / sqrt(1 − (1−x)²)` |
| Convex squircle | `(1 − (1−x)⁴)^(1/4)` | `(1−x)³ · (1 − (1−x)⁴)^(−3/4)` |
| Concave | `1 − convex(x)` | `−convex′(x)` |
| Lip | `mix(convex(x), concave(x), smootherstep(x))` | numeric |

`smootherstep(x) = 6x⁵ − 15x⁴ + 10x³`.

Note: both convex profiles have a **vertical tangent** at `x = 0` (infinite slope). Clamp the slope and feather the outer edge with `edgeSoftness` (§7).

Haze names the same options `Circle` (default), `Squircle`, `Lip`, `Concave` but publishes **no formulas** [C] https://github.com/chrisbanes/haze/blob/main/docs/effects/glass/index.md.

```glsl
// profile: 0 convex, 1 concave, 2 lip
// base:    0 circle, 1 squircle
float hBase(int base, float x)
{
    float a = 1.0 - x;
    return base == 0
        ? sqrt(max(1.0 - a * a, 0.0))                     // convex circle
        : pow(max(1.0 - a * a * a * a, 0.0), 0.25);       // convex squircle
}

float hProfile(int profile, int base, float x)
{
    float convex = hBase(base, x);
    if (profile == 0) return convex;
    if (profile == 1) return 1.0 - convex;               // kube.io's concave convention
    float t = x * x * x * (x * (x * 6.0 - 15.0) + 10.0); // smootherstep
    return mix(convex, 1.0 - convex, t);                 // lip
}

// Numeric derivative (kube.io uses delta = 0.001).
float hPrime(int profile, int base, float x)
{
    const float e = 1e-3;
    float x0 = clamp(x - e, 0.0, 1.0);
    float x1 = clamp(x + e, 0.0, 1.0);
    return clamp((hProfile(profile, base, x1) - hProfile(profile, base, x0)) / (x1 - x0), -6.0, 6.0);
}
```

[P] The concave convention `1 − convex(x)` in kube.io starts high at the outer edge and falls inward. I have not verified its geometry against a real concave bezel. Check the sign of `hPrime` before trusting `Concave`.

### 2.3 Height field and normal

Let `d` be the inward distance in px, so `x = d / bezelWidth` (clamped to [0,1]). Let `H` be the total height above the backdrop, in px:

```
H(d) = slabThickness + bezelRise · h(x)
```

- `slabThickness` [E]: the uniform glass depth. It sets the **magnitude** of offsets but not the zero at the centre (see §2.4).
- `bezelRise` [E]: the extra height at the bezel top. Haze's `refractionHeightFraction` (0.15 Regular, 0.35 Clear) is the closest documented analogue. Its exact definition is not stated [C].

Gradient in screen space:

```
∇H = (bezelRise / bezelWidth) · h′(x) · gradInward        (gradInward = sdfInwardDir(...))
N  = normalize(vec3(−∇H.x, −∇H.y, 1))
```

`N` points toward the viewer. It tilts outward at the edge (outer `N.xy` points away from the centre).

### 2.4 Snell's law and displacement

**Law** [P]: `n₁ sin θ₁ = n₂ sin θ₂`, with `n₁ = 1` (air) and `n₂ = n_glass`.

Orthographic eye rays travel straight down, so `I = (0, 0, −1)`.
`refract(I, N, 1/n)` is the vector form.

```glsl
const vec3 VIEW_I = vec3(0.0, 0.0, -1.0);   // orthographic eye ray, toward the glass

// Returns the backdrop sample offset (px) for a surface point at height H.
vec2 refractionOffset(vec3 N, float nGlass, float H)
{
    vec3 T = refract(VIEW_I, N, 1.0 / nGlass);  // Snell
    // T.z < 0 always for n > 1 with this incidence (no TIR)
    return T.xy / max(-T.z, 1e-4) * H;          // travel from surface height H down to the backdrop plane
}
```

Sampling: the pixel at `p` shows backdrop at `p + refractionOffset(...)`.

**Sign check (derived):** at the left edge of a convex dome, `N.x < 0` (outward). Then `T.x > 0`, so the sample comes from **inside** the glass. This is the classic edge compression, where the rim shows content pulled in from further away.

**Centre zero (derived):** at `N = (0,0,1)`, `refract(I, N, 1/n) = I`, so the offset is exactly zero for any `n` and any slab thickness. The centre is crisp and undistorted, which matches the existing project memory note on the centre being crisp.

**Magnitude:** `|offset| = H · tan θ_t`. Thicker glass gives larger offsets. Normalise to a designer-set maximum:

```
offset_px = refractionDisplacement · (H · tanθ_t) / max_over_bezel(H · tanθ_t)     [E]
```

This is the same normalise-to-max step kube.io uses (`maximumDisplacement`) [C]. It removes the thickness unit and gives a single knob.

**IOR** [C/P]:
- kube.io uses `n = 1.5` ("prefer 1.5 (glass)") [C].
- N-BK7 optical glass is `n_d = 1.5168` [P, standard catalogue value, not fetched this session].
- Apple's value is [U]. The usual range `1.45 – 1.52` is a sensible search space.
- Effect: IOR only scales the bend. Haze's `refractionStrength` and the displacement cap do most of the visual control [C].

### 2.5 Fresnel (Schlick)

```glsl
const float N_GLASS = 1.5;
const float F0 = pow((N_GLASS - 1.0) / (N_GLASS + 1.0), 2.0);   // = 0.04 for n = 1.5 [P]

float fresnelSchlick(float cosTheta)                 // cosTheta = N.z (view along -z)
{
    return F0 + (1.0 - F0) * pow(1.0 - clamp(cosTheta, 0.0, 1.0), 5.0);
}
```

[E] Use it to weight the rim and edge reflection. The result is near 0.04 at the flat centre and approaches 1 at grazing edges.

### 2.6 Haze numbers, for calibration [C]

| Parameter | Regular | Clear | Source |
|-----------|---------|-------|--------|
| Refraction profile | `Edge(20 dp)` | `Edge(28 dp)` | Haze Glass docs |
| Refraction strength | 0.7 | 0.85 | Haze Glass docs |
| Refraction displacement (max) | 48 dp | 56 dp | Haze Glass docs |
| Lighting height fraction | 0.15 | 0.35 | Haze Glass docs |
| Refraction fold strength | 0 (default) | — | Haze Glass docs |
| Surface profile | Circle (default) | Circle | Haze Glass docs |

Haze also states that displacement is **capped at half the shortest side**, so compact capsules refract less than large cards [C].

---

## 3. Chromatic aberration and dispersion

### 3.1 Physical model (Cauchy)

[P] Cauchy's equation: `n(λ) = A + B / λ²`, with `λ` in µm.

N-BK7 (standard optical crown glass): `A = 1.5046`, `B = 0.00420 µm²` [P, catalogue values, not fetched this session].

| Channel | λ (µm) | n(λ) |
|---------|--------|------|
| R | 0.656 | 1.51436 |
| G | 0.587 | 1.51679 |
| B | 0.486 | 1.52238 |

- `Δn_BR = n_B − n_R ≈ 0.0080`
- Abbe number `V_d = (n_d − 1) / (n_F − n_C) ≈ 64` for N-BK7 [P].
- Relative deflection spread `Δn / (n − 1) ≈ 0.0080 / 0.5168 ≈ 1.5 %`.

So **real** glass dispersion across the R–B span is about **1.5 %** of the refraction offset. At a 48 px maximum this is only about 0.7 px of fringe. Apple's visible fringing is therefore **artistic amplification** of a physical effect [E], not a literal copy of N-BK7.

### 3.2 Where it appears

[P] Because `refract(I, N, ·) = I` at `N = (0,0,1)`, dispersion is **automatically zero at the flat centre** and grows with bezel tilt. This matches the brief's "strictly in the curved bezel" requirement. No extra mask is needed for the physical model.

[D] Apple does not document fringing. [C] Haze lists chromatic aberration as "prismatic splitting at edges" [C].

### 3.3 Shader implementation

```glsl
float cauchyIOR(float lambdaUm)             // N-BK7
{
    return 1.5046 + 0.00420 / (lambdaUm * lambdaUm);
}

// Per-channel backdrop sample offsets. u_dispersion = artistic gain; 1.0 = physical.
void dispersedOffsets(vec3 N, float H, float gain,
                      out vec2 oR, out vec2 oG, out vec2 oB)
{
    float nG = cauchyIOR(0.587);
    float nR = nG + (cauchyIOR(0.656) - nG) * gain;   // amplify spread around nG
    float nB = nG + (cauchyIOR(0.486) - nG) * gain;
    oR = refractionOffset(N, nR, H);
    oG = refractionOffset(N, nG, H);
    oB = refractionOffset(N, nB, H);
}
```

Sample `sharpTex` at `p + oR` (red), `p + oG` (green), and `p + oB` (blue). The existing shader already does a red/green/blue split with `u_dispersion`, so this is a drop-in refinement.

[E] Starting gain: `1.0` physical; `4–12` for a visible fringe. Haze's tip is "start at 0.1–0.25 to avoid rainbow artifacts" for its own strength scale [C]. The two scales differ, so tune by eye.

---

## 4. Blur, frost, diffusion and colour grading

### 4.1 What is documented

- [D] Regular: "Blurs and adjusts the luminosity of background content to maintain legibility." [HIG Materials]
- [D] Clear: "highly translucent," for "visually rich" backgrounds [HIG Materials].
- [D] "Reduce Transparency makes Liquid Glass frostier and obscures more of the content behind it." [WWDC25 219]
- [D] "The amount of tint and the dynamic range shift to always ensure buttons remain legible." [WWDC25 219]
- [D] "Tinting should only be used to bring emphasis to primary elements and actions." [WWDC25 219]
- [D] Tint: "Selecting a color generates a range of tones that are mapped to content brightness underneath the tinted element." [WWDC25 219]
- [D] Dimming: "If the underlying content is bright, consider adding a dark dimming layer of 35% opacity." [HIG Materials, the only numeric Apple value]

### 4.2 Order: blur before or after refraction?

- [C] Linear's approximation stacks a Gaussian base blur, a gradient, then a specular shader [C].
- [C] Haze: "blur and depth grow with the surface's shortest side" for Regular, with a "fixed shallow blur" for Clear [C].
- [C] kube.io refracts the source and does not blur it [C].

[E] Recommended order, consistent with the project memory ("never just blurry", crisp lensing):

1. **Refract** the sharp backdrop (`sharpTex`) through the bezel offsets.
2. **Frost** by mixing toward the blurred backdrop (`blurTex`) with a weight that depends on size and centre distance.
3. **Grade** colour and tone.
4. **Add** specular, rim, and shadow.

Sampling the blurred texture at the **refracted** coordinate keeps the lens shape while diffusing content.

### 4.3 Size-dependent frosting

[C] Haze's Regular "blur and depth grow with the surface's shortest side," and Clear keeps "the same shallow blur at every size" [C].
[D] WWDC: larger elements gain "deeper, richer shadows," "more pronounced lensing and refraction," and "a softer scattering of light" [WWDC25 219, Adaptivity].

```glsl
float s = min(u_glassRect.z, u_glassRect.w);           // shortest side (px)
float sizeT = smoothstep(120.0, 480.0, s);              // [E] ramp range
float frost = mix(0.25, 1.0, sizeT) * u_frostAmount;    // [E]
vec3  bg    = mix(sharp, blurred, frost);
```

### 4.4 Colour grading

```glsl
const vec3 LUMA = vec3(0.2126, 0.7152, 0.0722);         // Rec.709

vec3 grade(vec3 c, float sat, float lift)
{
    float L = dot(c, LUMA);
    c = mix(vec3(L), c, sat);                           // sat > 1 boosts saturation
    return c + lift;
}
```

[C] Haze's `chromaMultiplier > 1` boosts saturation without clipping in-gamut colours [C]. Its value is not specified.

### 4.5 Adaptive legibility (light / dark)

[D] Regular "appears darker over dark backgrounds and lighter over light ones" [HIG Materials, image description]. Clear needs a dimming layer over bright content [D].
[D] Symbols and glyphs "flip from light to dark and vice versa, mirroring the glass's behavior" [WWDC25 219].
[D] Small elements adapt constantly. Bigger elements "adapt based on context, but they don't flip from light to dark" [WWDC25 219].

```glsl
float Lbg = dot(blurredBg, LUMA);                              // local backdrop luminance
float bright = smoothstep(0.55, 0.80, Lbg);                    // [E] thresholds

// Clear: dim bright content (HIG 35 %).
float clearDim = 0.35 * bright * u_clearAmount;                // [D] 35 % + [E] threshold
vec3 scrimed   = mix(bg, vec3(0.0), clearDim);

// Regular: tone follows the backdrop, dark over dark and light over light.
float followAmt = u_adaptAmount;                                // [E] 0.2 – 0.4
vec3 toneTarget = mix(u_darkTone, u_lightTone, smoothstep(0.2, 0.8, Lbg));
vec3 toned      = mix(scrimed, toneTarget, followAmt);
```

`u_darkTone`, `u_lightTone`, and `u_adaptAmount` are [U]. Tune against screenshots.

[C] Haze's Regular dark style uses "a darker tone, edge, and lighting response, less backdrop contrast, and a fainter rim" [C].

---

## 5. Specular rim, Fresnel, glare, and internal reflections

### 5.1 Documented behaviour

- [D] "Light sources inside of this environment shine on the material producing highlights," which "respond to geometry just as you'd expect." [WWDC25 219, "Principles"]
- [D] "these lights move in space, causing light to travel around the material, defining its silhouette." [WWDC25 219]
- [D] "In some cases, the lighting responds to device motion." [WWDC25 219]
- [D] "the light reflects, scatters, and bleeds into the shadow as well." [WWDC25 219]
- [C] Linear: a "specular highlight calculated in a SwiftUI shader," fed by the **SDF gradient normal**, with a "Plus Lighter blend mode," masked to the same SDF [C].

Apple does **not** document a "top-left plus bottom-right" bounce. That description in the brief is not supported by a public source [U]. Haze puts rims on top and bottom only by default, with a `sides` option for all edges [C].

### 5.2 Specular (Blinn–Phong on the bezel normal)

```glsl
vec3 L = normalize(vec3(u_lightDir, 0.9));    // u_lightDir: xy toward the light; [E] top-left default
vec3 V = vec3(0.0, 0.0, 1.0);
vec3 Hh = normalize(L + V);
float specShininess = 40.0;                   // [E] 20 – 80
float spec = pow(max(dot(N, Hh), 0.0), specShininess) * u_specIntensity;   // Haze default 0.48 [C]
```

For a moving light (device motion or pointer), update `u_lightDir` each frame. Haze's `lightPosition` is `Center` by default [C].

### 5.3 Rim (thin light band, inside the edge)

```glsl
float dIn   = -continuousRoundedBoxSDF(p, b, R, n);     // px inside (> 0 inside)
float rimW  = 1.0;                                       // px [C] Haze width 1 dp
float band  = smoothstep(rimW, 0.0, dIn);
// Restrict to top and bottom unless "sides" is requested. Haze's sides = 0 by default [C].
vec2  outward = -sdfInwardDir(p, b, R, n);
float tb      = smoothstep(0.3, 0.9, abs(outward.y));
float side    = mix(tb, 1.0, u_rimSides);               // u_rimSides = 0 top/bottom, 1 all edges
float rim     = band * side * u_rimLight * (0.7 + 0.3 * fresnelSchlick(N.z));   // Haze intensity 0.7 Regular, 1.0 Clear [C]
```

### 5.4 Edge outline and edge shadow

[C] Haze's built-in outline is black, 0.7 dp wide, 58 % opacity. Its top/bottom strength is 0.42 for Regular (0.1 in dark) and 0 for Clear [C].
[C] Haze's edge shadow is a shape-following dark rim beneath the highlight, default black at 14 % opacity [C].
[C] Haze's `ambientResponse` (Fresnel edge lift) defaults to 0.15 [C].

```glsl
// Outline is drawn just outside the shape, so it is composited separately from the glass body.
float outlineMask = smoothstep(0.7, 0.0, abs(dIn));                       // [C] 0.7 px line on the edge
float outlineA    = 0.58 * mix(0.42, 1.0, abs(outward.x)) * outlineMask;  // [C] 58 % black, side-weighted
// Edge shade: 14 % black just inside the rim.
float edgeShade   = 0.14 * smoothstep(3.0, 0.0, dIn);                     // [C] 14 %, [E] 3 px fall-off
col = mix(col, vec3(0.0), edgeShade);                                     // body
col = mix(col, vec3(0.0), outlineA);                                      // outline (drawn over the body)
```

### 5.5 Internal reflection

[P] Total internal reflection needs `n₁ > n₂` going from glass to air. From the eye side, air to glass, it does not occur, so glass-side TIR is not in the front-face model. It can matter at the back face. Not implemented in this spec.

[E] Skip internal reflection. Fresnel plus a faint inner caustic is enough.

### 5.6 Drop shadow

[D] Larger elements cast "deeper, richer shadows" [WWDC25 219]. Shadows strengthen over text and fade over solid light backgrounds [WWDC25 219, Principles].
[U] No public numbers.

[E] Starting values for the outer shadow:

```
shadowOffsetY = 6 px     shadowBlur (sigma) = 14 px     shadowAlpha = 0.22
(scale all three with shortest side; cap alpha at 0.35)
```

Make the alpha respond to backdrop luminance: higher over text or bright areas, lower over solid light (WWDC wording), so `shadowAlpha *= mix(0.8, 1.2, bright)`.

---

## 6. Interactive and dynamic behaviour

### 6.1 Documented behaviour

- [D] "When you interact with Liquid Glass, the material illuminates from within as a form of feedback." [WWDC25 219, Principles]
- [D] The glow "starts right under your fingertips, the glow spreads throughout the element and onto any Liquid Glass elements nearby." [WWDC25 219]
- [D] "Liquid Glass responds to interaction by instantly flexing and energizing with light." [WWDC25 219, Dynamics]
- [D] "Elements can even lift up into Liquid Glass temporarily, such as when you interact with a component." [WWDC25 219]
- [D] "it has an inherent gel-like flexibility to it that communicates its transient and malleable nature." [WWDC25 219]
- [D] Appearance "materialize[s] in and out by gradually modulating the light bending and lensing," instead of fading. [WWDC25 219]
- [D] Reduce Motion "decreases the intensity of some effects and disables any elastic properties." [WWDC25 219]

### 6.2 Community implementation [C]

Haze's interaction blocks (`pressed`, `hovered`, `focused`) can set `lightingIntensity`, `refractionMultiplier`, `whitePointDelta`, and `scale`. `scale` accepts up to `1.2`, "similar to iOS interactive Liquid Glass." Pivot: `Pointer` or `Center`. Reduced-motion policy: `System`, `Reduced`, or `Full` [C].

### 6.3 Shader equations [E]

```glsl
// Pointer bulge: a soft radial push that bends the backdrop sample.
vec2  dP   = p - u_pointerPos;
float dist = length(dP);
float w    = exp(-dist * dist / (2.0 * u_pointerRadius * u_pointerRadius));
vec2  push = (dist > 1e-3 ? dP / dist : vec2(0.0)) * w * u_pointerBulge * u_pointerStrength;

// Touch illumination from within: additive glow under the pointer.
float glow = u_pointerLight * u_pointerStrength * w;
col += glow * vec3(1.0);
```

Use `push` in the refraction offset: `o_total = refractionOffset(...) + push`. The repo already has `u_pointerPos`, `u_pointerStrength`, `u_pointerLight`, `u_pointerBulge`, and `u_pointerRadius`.

**Spring for morph and lift** (critically damped feel, the "gel" response) [E]:

```
x'' = −k·(x − target) − c·x'        k = 320 s⁻², c = 2·sqrt(k)·0.65
```

Use `scale = 1 + 0.04 · press` as a starting value. Clamp to Haze's documented maximum of `1.2` [C]. Disable the spring under Reduce Motion [D].

### 6.4 Device motion light swing [D + E]

[D] Lighting "responds to device motion" in some cases [WWDC25 219].
[E] Map a tilt vector `(tx, ty)` to `u_lightDir`, clamped to ±0.6, with a low-pass filter (τ ≈ 0.25 s).

---

## 7. Edge softness and anti-aliasing

[C] Haze's `edgeSoftness` default is `1 dp` (0 gives hard edges). It "combine[s] with rounded shapes" [C].
[E] Starting value: `edgeSoftness = 1.0 px`.

```glsl
float aa   = fwidth(sdf);
float mask = 1.0 - smoothstep(-aa, aa + u_edgeSoftness, sdf);
```

---

## 8. Mapping to the repo shader

Candidate parameter table for `glass/src/shaders/glass.frag`. The existing uniforms are listed from the file at line 11–47. **Verify the semantics against the full shader before wiring any value.**

| Concept | Existing uniform | Regular start | Clear start | Source |
|---------|------------------|---------------|-------------|--------|
| Corner radius | `u_radius` | 16 px (continuous) | 16 px | [C] Haze |
| Squircle smoothing / `n` | (new) | smoothing 1 → n 4 | smoothing 1 → n 4 | [E] §1.1 |
| Bezel width | `u_bezel` | 20 px | 28 px | [C] Haze |
| Refraction max displacement | `u_refraction` | 48 px | 56 px | [C] Haze |
| Refraction strength | `u_refraction` (scale) | 0.7 | 0.85 | [C] Haze |
| Dispersion gain | `u_dispersion` | 1.0 physical, 4–12 visible | 4–12 visible | [P]/[E] §3 |
| Saturation | `u_saturation` | 1.1 | 1.05 | [E] |
| Brightness lift | `u_brightness` | 0.0 | 0.02 | [E] |
| Rim intensity | `u_rimLight` | 0.7 (0.25 dark) | 1.0 | [C] Haze |
| Rim width | `u_rimWidth` | 1 px | 1 px | [C] Haze |
| Rim sides | (new) | 0 top/bottom | 0 | [C] Haze |
| Specular | `u_sheen` | 0.48 | 0.48 | [C] Haze |
| Edge shade (14 % black) | `u_edgeShade` | 0.14 | 0.14 | [C] Haze |
| Outline width / alpha | `u_borderWidth` | 0.7 px at 58 % | none | [C] Haze |
| Outline top/bottom strength | (new) | 0.42 | 0 | [C] Haze |
| Ambient Fresnel | (new) | 0.15 | 0.15 | [C] Haze |
| Edge softness | (new) | 1 px | 1 px | [C] Haze |
| Pointer bulge / light / radius | `u_pointerBulge`, `u_pointerLight`, `u_pointerRadius` | [E] | [E] | [E] §6 |
| Shadow | `u_shadowColor`, `u_shadowBlur`, `u_shadowOffset` | alpha 0.22, blur 14, offsetY 6 | same | [E] §5.6 |
| Clear dim over bright | (new) | — | 0.35 when bright | [D] HIG |

Unit note: Haze's values are in `dp`. Convert with the display scale factor, keeping the offsets in physical pixels.

---

## 9. Implementation checklist for `glass.frag`

1. Replace the current bezel with the analytic `continuousRoundedBoxSDF` (§1.2). Keep `fwidth` AA.
2. Compute `h`, `h′`, and `∇H` (§2.2–2.3) with a selectable profile (`u_profile`: convex / concave / lip; `u_base`: circle / squircle).
3. Compute `N` and call `dispersedOffsets` (§3.3) for R, G, and B. Sample `sharpTex` at `p + oR/oG/oB`.
4. Mix in `blurTex` by the size-dependent `frost` (§4.3).
5. Grade (§4.4), then apply the adaptive tone and Clear dim (§4.5).
6. Add rim, specular, outline, edge shade, and Fresnel (§5).
7. Add pointer and light effects (§6.3) and the outer shadow (§5.6).
8. Keep `dispersion = 0` at the centre automatically (it follows from `refract`). Do not add an extra mask.

Performance: three `refract` calls and three texture reads per pixel, plus one blur sample. Keep `h′` numeric only for the Lip profile. Use analytic derivatives for circle and squircle.

---

## 10. Open questions and what would settle them

1. [U] Apple's exact IOR, bezel shape, bezel width, and slab thickness. **Settle by:** side-by-side comparison with the user's own macOS 26 screenshots, with the same wallpaper and window size.
2. [U] Apple's dispersion amount and whether it is per-channel or a single fringe. **Settle by:** the same comparison, looking at high-contrast edges.
3. [U] Blur sigma, saturation, brightness and tone values. **Settle by:** colour-picker sampling of the user's screenshots, under known backdrops.
4. [U] Whether specular is top-left/bottom-right or top-only. HIG and WWDC say "light sources move," so the default light is not documented. **Settle by:** screenshot comparison with a moving light.
5. [U] Corner exponent `n` (4 vs 5 vs Bézier). **Settle by:** overlaying the SDF on a high-zoom screenshot corner.
6. [U] Outer shadow numbers. **Settle by:** shadow falloff measurement on a light and a dark wallpaper.

---

## 11. Sources

**Read in full this session**

- kube.io, "Liquid Glass in the Browser: Refraction with CSS and SVG" — https://kube.io/blog/liquid-glass-css-svg/ (profiles, Snell, normalisation, encoding; read via web-fetch)
- Haze, Glass effect documentation — https://github.com/chrisbanes/haze/blob/main/docs/effects/glass/index.md (all parameters and defaults; no shader source)
- WWDC25 session 219 "Meet Liquid Glass" transcript — https://nonstrict.eu/wwdcindex/wwdc2025/219/ (no timestamps or numbers in this transcript)
- Apple HIG, Materials — https://developer.apple.com/design/human-interface-guidelines/materials.md (35 % dimming; regular vs clear)
- Apple technology overview, Liquid Glass — https://developer.apple.com/documentation/technologyoverviews/liquid-glass.md (no numbers)
- Linear, "Liquid Glass" write-up (Robb Böhnke, 2025-10-21) — https://linear.app/blog/linear-liquid-glass (no refraction; states refraction needs pixel-level data unavailable to third-party apps)
- John D. Cook, "Apple design, squircles, and curvature" — https://www.johndcook.com/blog/2018/02/13/squircle-curvature/ (superellipse forms, p = 4 vs 5)
- squircle.js, "The Squircle Formula" — https://squircle.js.org/blog/math-behind-squircles (cornerSmoothing, iOS icon remarks)

**Seen but not usable**

- Haze liquid-glass page — https://chrisbanes.github.io/haze/latest/effects/liquid-glass/ returned **404**. The Glass page above replaces it.
- Apple developer video — https://developer.apple.com/videos/play/wwdc2025/219/ (not fetched; the transcript mirror had the content)

**Used from general knowledge, not fetched this session** [P]

- N-BK7 Sellmeier / Cauchy constants (`A = 1.5046`, `B = 0.00420 µm²`, `n_d = 1.5168`, `V_d ≈ 64`). Verify against the Schott catalogue before citing externally.
- Schlick Fresnel approximation (Schlick 1994).
- Superellipse and Lamé curve definitions.

---

## 12. Bottom line (for the implementer)

- **Surface:** superellipse / Lamé rounded box, `n ≈ 4` at full smoothing [C/E].
- **Geometry:** lensing lives on a tilted bezel. The flat centre is crisp and has zero displacement [P, derived].
- **Optics:** Snell with `n ≈ 1.5` (range 1.45–1.52); offset `= refract(I, N, 1/n).xy / (−T.z) · H`, normalised to a max displacement [C/P].
- **Dispersion:** Cauchy N-BK7 gives about 1.5 % real spread. Visible fringes need an artistic gain [P/E]. Zero at the centre, automatically.
- **Frost:** refract the sharp backdrop, then mix toward blur by size. Keep the lensing crisp [C/E].
- **Colour:** saturation boost, luminance-following tone for Regular, and a 35 % dim over bright content for Clear [D/C/E].
- **Highlights:** Blinn–Phong on the bezel normal, a 1 px top/bottom rim, 14 % edge shade, and 0.7 outline [C/E].
- **Interaction:** pointer bulge plus additive glow; elastic spring on press; disabled under Reduce Motion [D/E].
- **Parity:** cannot be verified from public data. Calibrate against the user's own macOS 26 screenshots [U].
