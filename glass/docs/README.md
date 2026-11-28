# Fluctus Glass: engineering docs

The deep material behind [Fluctus Glass](../README.md) (KWin effect id `fluctusglass`).

1. [liquid-glass-math.md](liquid-glass-math.md): the optics and shader specification: surface
   SDF, bezel height field and Snell refraction, dispersion, frost and color grading, specular
   rim, interaction, and open questions.
2. [apple-liquid-glass-re.md](apple-liquid-glass-re.md): what is known about Apple's Liquid Glass
   (iOS 26 / macOS 26). It separates verified findings from documentation, observation and
   guesswork.

## How the pieces fit

```text
 app ── GlassArea / GlassRequest ──D-Bus──▶ glass/src (KWin effect, GPL-3)
        (libs/glassclient, Apache-2.0)        │  picks windows, shapes and material
                                              ▼
                                   crates/liquidglass (Rust, Apache-2.0)
                                     renders into KWin's GL framebuffer, zero-copy
```

- `glass/src/glassservice.*` serves the D-Bus protocol from
  [GlassProtocol.hpp](../../libs/glassclient/include/FluctusGlass/GlassProtocol.hpp).
- `glass/src/shapes.*` and `material.*` hold the pure logic, unit-tested without KWin
  (`just glass test`).
- `glass/src/glasseffect.*` is the KWin adapter. It handles global mode, per-window state, the
  Kawase backdrop passes, and the call into `lg_gl_render`.
- `glass/tools/` holds the nested-KWin runner (`just glass nested`) and the visual parity scorer
  (`just glass parity`). The Apple reference images it compares against are not in the
  repository; put them in `$FLUCTUS_PARITY_REFS` (default `~/.cache/fluctus/parity-refs`).
