# macOS Dock Animation Physics & UX Implications

Replicating the feel of the macOS Dock requires solving **four coupled problems** together:
1. Smooth, finite-support magnification curve.
2. Exact, oscillation-free neighbor displacement anchored directly at the cursor (Fitts's Law).
3. Stable, low-latency temporal smoothing (avoiding double-filtering lag and negative-scale overshoot).
4. Crisp high-DPI rendering via scale-only transforms (zero per-frame re-rasterization).

---

## 1. Magnification Curves

Let:
- $s_0$ = base (resting) icon size in logical pixels (e.g., `48 px`)
- $g$ = resting gap between adjacent icons (e.g., `6 px`)
- $M \ge 1.0$ = maximum scale factor (e.g., `1.8` for +80% magnification)
- $R > 0$ = influence radius in resting logical pixels (e.g., `2.5 * (s_0 + g)`)
- $\alpha \in [0, 1]$ = smoothed hover engagement factor (ramps $0 \to 1$ on dock enter, $1 \to 0$ on exit)

For a normalized resting-space distance $u = \min(|r - p| / R, 1.0) \in [0, 1]$:

| Curve | Weight $w(u)$ on $[0, 1]$ | Continuity at $u = 1$ | Closed-Form Antiderivative $W(u) = \int_0^u w(t)\,dt$ |
| --- | --- | --- | --- |
| **Raised Cosine (Default, macOS-like)** | $\frac{1 + \cos(\pi u)}{2}$ | **$C^1$** ($w(1) = 0, w'(1) = 0$) | $\frac{1}{2}\left(u + \frac{\sin(\pi u)}{\pi}\right)$ |
| **Quintic Smootherstep** | $1 - (6u^5 - 15u^4 + 10u^3)$ | **$C^2$** ($w, w', w'' = 0$ at $1$) | $u - u^6 + \frac{15}{5}u^5 - \frac{10}{4}u^4 = u - u^6 + 3u^5 - 2.5u^4$ |
| **Windowed Gaussian (WaveTask-like, optional)** | $\frac{e^{-k u^2} - e^{-k}}{1 - e^{-k}}$ ($k = 4.5$) | **$C^0$ only** ($w(1) = 0$, slight slope kink $w'(1) \approx -0.1$) | $\frac{\frac{\sqrt{\pi}}{2\sqrt{k}}\operatorname{erf}(\sqrt{k}\,u) - u e^{-k}}{1 - e^{-k}}$ |

All three curves are strictly $0$ for $u \ge 1$ (finite support), and all three have exact closed-form integrals — zero numerical quadrature needed.

---

## 2. Continuous Scale-Density Integral (Zero Drift, Zero Overlap)

A naive layout that maps the animated dock back into resting coordinates (`[x0, x0 + L_total] → [0, L_rest]`) or center-anchors a magnified row creates a **feedback loop**: animated scales shift the icon under the cursor, which changes the distance to the cursor, causing drift and oscillation.

### The Exact Solution: Resting-Space Density Integral
Define each icon $i \in [0, N-1]$ by its **static resting interval** $[a_i, b_i]$ along the primary axis:
$$a_i = i \cdot (s_0 + g), \qquad b_i = a_i + s_0, \qquad L_{\text{rest}} = N s_0 + \max(0, N - 1) g$$

Given pointer position $p$ sampled directly against the **static resting layout** (clamped to $[0, L_{\text{rest}}]$), define the continuous excess scale density:
$$\sigma(r) = (M - 1) \cdot \alpha \cdot w\!\left(\frac{|r - p|}{R}\right) \ge 0$$

Map any resting coordinate $r$ to its magnified display coordinate $X(r)$ via:
$$X(r) = p + (r - p) + \int_p^r \sigma(t)\,dt = r + \operatorname{sign}(r - p) \cdot (M - 1)\alpha R \cdot W\!\left(\min\!\left(\frac{|r - p|}{R},\, 1\right)\right)$$

### Why This Solves Every Layout & Fitts's Law Problem Simultaneously
1. **Zero cursor drift:** $X(p) = p$ identically. The exact point under the cursor never moves regardless of where $p$ is along the dock!
2. **Strictly monotonic (zero overlap):** Because $X'(r) = 1 + \sigma(r) \ge 1 > 0$, adjacent icons can never collide or invert, and no feedback oscillation is possible.
3. **Exact icon spans & scales:** Icon $i$ occupies $[X(a_i), X(b_i)]$, so its span is $L_i = X(b_i) - X(a_i)$ and its uniform scale factor is the exact average density over the icon:
   $$S_i = \frac{L_i}{s_0} = 1 + \frac{1}{s_0}\int_{a_i}^{b_i} \sigma(t)\,dt$$
   And the gap between icon $i$ and $i+1$ is automatically $X(a_{i+1}) - X(b_i) \ge g$.
4. **Clean pointer exit:** When the pointer leaves the dock hover hull, **freeze $p$** and let $\alpha \to 0$ smoothly; the dock collapses gracefully back to resting coordinates without snapping sideways.
5. **Exact primary-axis slack bound (for Wayland pre-allocation):**
   Because the pill grows unevenly on each side when hovering near an edge, each side can expand by at most:
   $$\Delta_{\max} = (M - 1) \cdot R \cdot W(1)$$
   (For Raised Cosine, $W(1) = \tfrac{1}{2}$, so $\Delta_{\max} = \frac{1}{2}(M - 1)R$ per side, and total maximum growth is $(M - 1)R$.)
   Pre-allocating $\Delta_{\max}$ on each side of the resting dock length guarantees **zero Wayland panel surface resizes** during hover — only the visible dock pill and blur region animate inside the pre-allocated plasmoid area.

### Fitts's Law Edge Depth
Hit-testing uses each icon's rendered span $[X(a_i), X(b_i)]$ extended all the way to the screen edge (`Plasmoid.constraintHints: CanFillArea`), so clicking at the exact edge pixel of the monitor always activates the icon above it.

---

## 3. Temporal Physics: Spring-Damper Integration

### Avoiding Double-Filtering Lag & Negative Overshoot
- **Pointer tracking:** macOS tracks pointer motion almost instantaneously; filtering *both* the pointer and every icon scale adds sluggish lag. By default, pointer position $p$ is **instant** (or very stiff, optional), while **hover engagement $\alpha$** and **insertion/removal slot weights** are driven by damped springs. Optional per-icon scale smoothing can be enabled as a configurable preset.
- **No negative overshoot on scales/engagement:** Under-damping ($\zeta < 1$) on $\alpha$ or $S_i$ can overshoot below $0$ or $1.0$, shrinking icons below resting size or creating negative gaps. Clamp $\alpha \in [0, 1]$ and use **critically damped springs ($\zeta \ge 1.0$)** for $\alpha$ and slot weights; reserve under-damping for bounce/attention effects if desired.

### Numerically Safe Analytical Spring Solver
For $\ddot{y} = \omega^2 (y_{\text{target}} - y) - 2 \zeta \omega \dot{y}$ over step $\Delta t$ (sourced from Qt's animation clock, clamped to $\Delta t \le 1/30\,\text{s}$ to prevent explosion across frame hitches — note that a hitch longer than $33\,\text{ms}$ will gently slow that frame rather than teleport):
- **Critically damped regime ($|\zeta - 1| < 10^{-4}$):** Use the exact critical formula $e^{-\omega \Delta t}\left(y_0 + (v_0 + \omega y_0)\Delta t\right)$. This avoids the $1/(r_1 - r_2)$ catastrophic cancellation of the overdamped formula as $\zeta \to 1$.
- **Underdamped ($\zeta < 1 - 10^{-4}$):** Exact $\cos(\omega_d \Delta t)$ / $\sin(\omega_d \Delta t)$ form with $\omega_d = \omega\sqrt{1 - \zeta^2}$.
- **Overdamped ($\zeta > 1 + 10^{-4}$):** Exact $e^{r_1 \Delta t}$ / $e^{r_2 \Delta t}$ form.
- **Settle thresholds in device pixels:** Mark a spring settled when $|y - y_{\text{target}}| \cdot s_{\text{scale}} \cdot \text{dpr} < 0.25\,\text{px}$ and velocity is negligible, then snap resting coordinates to device pixels and stop the animation driver (`0%` idle CPU/GPU).

---

## 4. Bounce Physics & Panel Headroom Formula

Icons bounce along the cross-axis (away from the screen edge) while `IsStartup`, `IsDemandingAttention`, or `smartLauncherItem.urgent` is true:
- **Latched hop height:** At the start of each hop ($u = 0$), latch the hop amplitude:
  $$H = \min\!\left(\text{bounceRatio} \cdot s_0 \cdot S_i,\; H_{\text{available}}\right)$$
  Latching $H$ at takeoff prevents the icon from jumping mid-air if the user hovers/unhovers while it is bouncing, and clamping to $H_{\text{available}}$ guarantees the icon never clips the panel ceiling.
- **Parabolic gravity arc:** $h(u) = 4 H \cdot u(1 - u)$ for $u \in [0, 1]$, followed by a short rest pause before the next hop. When attention/startup clears, the current hop finishes its descent cleanly ($h \to 0$).
- **Exact required panel thickness:**
  $$T_{\text{required}} = \text{edgePadding} + \lceil s_0 \cdot M \rceil + \lceil H_{\max} \rceil + \text{topMargin}$$
  For example, with $s_0 = 48\,\text{px}$, $M = 1.8$ ($86.4\,\text{px}$), and $\text{bounceRatio} = 0.55$ ($H_{\max} \approx 48\,\text{px}$), $T_{\text{required}} \approx 134\,\text{px} + \text{padding}$ (not $86\text{–}110\,\text{px}$). Both the layout template and the settings UI will compute this from the same C++ helper function.

---

## 5. High-DPI Crispness, Scale-Only Transforms & Reflections

- **Never call `setSize` per frame:** Resizing `Kirigami.Icon` every frame triggers expensive icon re-rasterization on every frame. Instead, each delegate has a **fixed logical size** of $\lceil s_0 \cdot M \rceil$ and is driven **only by `setScale(S_i / M)` and `setPosition(...)`** from C++.
- **Per-edge `transformOrigin`:** Set `transformOrigin` to `Item.Bottom` (BottomEdge), `Item.Top` (TopEdge), `Item.Left` (LeftEdge), or `Item.Right` (RightEdge) so scaling grows strictly away from the screen edge rather than into it.
- **Mipmapping & rest sharpness:**
  - When $M > 2.0$ (downscale factor $< 0.5$ at rest), plain bilinear filtering shimmers; enable `layer.mipmap: true` on the icon surface when $M > 1.8$.
  - Optionally swap to native pixel-aligned $s_0$ rasterization when the dock is completely settled at rest ($\alpha = 0$), and snap settled icon positions to device pixels.
  - Recompute raster size whenever `window->devicePixelRatio()` or the assigned screen changes.
- **Reflection:** Mirrored across the dock edge axis with a soft opacity mask and bounce compensation (as an icon lifts away by $h$, its reflection recedes by $h$ and fades proportionally).

---

## 6. User-Configurable Parameters

- Base icon size ($s_0$: `24–96 px`)
- Maximum magnification ($M$: `1.0× – 2.5×`, `1.0×` disables zoom)
- Influence radius ($R$: `1.0 – 4.0` icon pitches)
- Magnification curve (`Raised Cosine`, `Smootherstep`, `Windowed Gaussian`)
- Responsiveness preset (`Snappy`, `macOS Default`, `Fluid`, or custom spring $\omega$)
- Icon spacing, dock pill padding & corner radius, blur toggle, reflection toggle, bounce toggle & height ratio.
