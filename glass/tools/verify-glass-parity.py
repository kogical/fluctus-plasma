#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Kogical
# SPDX-License-Identifier: GPL-3.0-or-later
"""Quantitative visual parity test suite for Fluctus Glass against Apple Liquid Glass.

Renders the real compiled KWin `fluctusglass.so` compositor effect inside headless nested
`kwin_wayland` across multiple test cases:
  1. `tahoe_card`: Exact 1:1 registered macOS 26 Tahoe Light wallpaper + Apple Music card
     geometry (no text/icons on our card; Apple UI masked out for pure-glass pixel metrics).
  2. `control_center`: Multi-shape Control Center layout (slider pills, square tile, circles)
     over the Tahoe Light wallpaper, compared against Apple's Newsroom Control Center capture.
  3. `optical_grid`: Clean geometric grid + color-bar stress test (zero text/icons) to verify
     refraction smoothness, two-zone lensing, dispersion, rim directionality, and edge hygiene.

Usage:
  glass/tools/verify-glass-parity.py --build build/release --out /tmp/parity_out [--cases all]
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any

import numpy as np
from PIL import Image, ImageDraw

REPO_ROOT = Path(__file__).resolve().parents[2]
RUN_NESTED = REPO_ROOT / "glass/tools/run-nested.sh"
HARNESS_QML = REPO_ROOT / "glass/tools/ParityHarness.qml"

# Default local paths for cached Apple references (can be overridden by CLI args)
DEFAULT_REF = Path(
    "/home/natix/.t3/userdata/attachments/"
    "0c96d757-1b36-4c10-ad28-bbe2f7f357b8-e42b4528-b8ab-40af-8619-05fb7e33f998.png"
)
DEFAULT_GT = Path(
    "/tmp/claude-1000/-home-natix-Develop-github-fluctus-plasma/"
    "320ef985-2daf-4089-8e09-fa9ed3688e83/scratchpad/analysis/gt_wallpaper.png"
)
DEFAULT_CC_REF = Path(
    "/tmp/claude-1000/-home-natix-Develop-github-fluctus-plasma/"
    "320ef985-2daf-4089-8e09-fa9ed3688e83/scratchpad/refs/tahoe_cc.jpg"
)
DEFAULT_WP = Path(
    "/tmp/claude-1000/-home-natix-Develop-github-fluctus-plasma/"
    "320ef985-2daf-4089-8e09-fa9ed3688e83/scratchpad/wp/tahoe-light.png"
)


def sd_round_rect(
    px: np.ndarray, py: np.ndarray, cx: float, cy: float, hw: float, hh: float, r: float, n: float = 2.2
) -> np.ndarray:
    qx = np.abs(px - cx) - hw + r
    qy = np.abs(py - cy) - hh + r
    out = np.hypot(np.maximum(qx, 0.0), np.maximum(qy, 0.0)) + np.minimum(np.maximum(qx, qy), 0.0) - r
    corner = (qx > 0) & (qy > 0)
    ux = np.maximum(qx, 0.0) / r
    uy = np.maximum(qy, 0.0) / r
    s = np.power(np.power(ux, n) + np.power(uy, n), 1.0 / n)
    return np.where(corner, (s - 1.0) * r, out)


def compute_ssim(a: np.ndarray, b: np.ndarray, mask: np.ndarray) -> float:
    """Global luminance + per-channel structural similarity over masked pixels."""
    c1 = (0.01 * 255.0) ** 2
    c2 = (0.03 * 255.0) ** 2
    vals = []
    for ch in range(3):
        x = a[..., ch][mask]
        y = b[..., ch][mask]
        mx, my = float(x.mean()), float(y.mean())
        vx, vy = float(x.var()), float(y.var())
        cov = float(((x - mx) * (y - my)).mean())
        ssim_c = ((2 * mx * my + c1) * (2 * cov + c2)) / ((mx * mx + my * my + c1) * (vx + vy + c2))
        vals.append(ssim_c)
    return float(np.mean(vals))


def make_1280x800_tahoe_backdrop(gt_path: Path, out_path: Path) -> tuple[int, int, int]:
    """Place the 1600x1200 registered Tahoe wallpaper at exact 2/3 scale (1067x800) inside 1280x800."""
    gt = Image.open(gt_path).convert("RGB")
    sw, sh = 1067, 800
    gt_scaled = gt.resize((sw, sh), Image.LANCZOS)
    canvas = Image.new("RGB", (1280, 800))
    x_off = (1280 - sw) // 2  # 106
    # Fill left/right margins by extending edge columns cleanly
    arr = np.asarray(gt_scaled)
    full = np.zeros((800, 1280, 3), dtype=np.uint8)
    full[:, :x_off] = arr[:, :1]
    full[:, x_off : x_off + sw] = arr
    full[:, x_off + sw :] = arr[:, -1:]
    Image.fromarray(full).save(out_path)
    return x_off, sw, sh


def make_optical_grid_backdrop(out_path: Path) -> None:
    """Create a crisp high-contrast optical test chart (grid + diagonal stripes + color bars)."""
    W, H = 1280, 800
    img = np.zeros((H, W, 3), dtype=np.uint8)
    yy, xx = np.mgrid[0:H, 0:W]

    # Top half: rich color gradient bars + crisp white/dark grid lines
    # Smooth diagonal blue/teal/gold/magenta backdrop with crisp 40px grid
    t = (xx / W * 0.7 + yy / H * 0.3)
    img[..., 0] = np.clip(30 + 190 * np.sin(t * np.pi * 1.4) ** 2, 0, 255).astype(np.uint8)
    img[..., 1] = np.clip(60 + 165 * np.sin((t + 0.35) * np.pi * 1.2) ** 2, 0, 255).astype(np.uint8)
    img[..., 2] = np.clip(110 + 135 * np.cos(t * np.pi * 0.9) ** 2, 0, 255).astype(np.uint8)

    # Diagonal high-contrast dark/bright ribbon across the card
    diag = (xx - 0.8 * yy) % 160
    ribbon = (diag < 38)
    img[ribbon] = np.array([15, 38, 165], dtype=np.uint8)
    bright_line = (diag >= 38) & (diag < 46)
    img[bright_line] = np.array([235, 245, 255], dtype=np.uint8)

    # Crisp 40px grid lines
    grid = ((xx % 40) < 2) | ((yy % 40) < 2)
    img[grid] = np.clip(img[grid].astype(np.int32) + 55, 0, 255).astype(np.uint8)

    # Vertical color bars on right half for pill & circle refraction inspection
    bars = [
        (660, 740, (240, 235, 210)),
        (740, 820, (30, 95, 225)),
        (820, 900, (55, 200, 220)),
        (900, 980, (220, 65, 130)),
        (980, 1060, (20, 35, 95)),
        (1060, 1140, (245, 185, 65)),
        (1140, 1220, (40, 165, 120)),
    ]
    for x0, x1, col in bars:
        img[100:520, x0:x1] = np.array(col, dtype=np.uint8)
        # horizontal 36px stripes across bars
        stripe = ((yy[100:520, x0:x1] % 36) < 4)
        img[100:520, x0:x1][stripe] = np.array([250, 250, 255], dtype=np.uint8)

    Image.fromarray(img).save(out_path)


def run_nested_capture(build_dir: Path, mode: str, backdrop: Path, out_shot: Path) -> None:
    """Invoke headless nested KWin with ParityHarness.qml in the requested mode."""
    with tempfile.TemporaryDirectory(prefix=f"fluctus-parity-{mode}-") as tmp:
        tmp_path = Path(tmp)
        wrapper = tmp_path / "RunHarness.qml"
        wrapper.write_text(
            f'import "file://{HARNESS_QML.parent}"\n'
            f'ParityHarness {{ mode: "{mode}" }}\n',
            encoding="utf-8",
        )
        env = os.environ.copy()
        env["BACKDROP_IMAGE"] = str(backdrop)
        env["ANIMATE"] = "false"
        env["BLOCKS"] = "false"
        env["SHOW_TEXT"] = "false"
        env["FRAMELESS"] = "true"
        env["DEMO_QML"] = str(wrapper)
        env["FLUCTUS_NESTED_STAGE"] = str(tmp_path / "stage")
        cmd = ["timeout", "-k", "5", "60", str(RUN_NESTED), str(build_dir), "--screenshot", str(out_shot)]
        log_file = tmp_path / "run.log"
        with log_file.open("w", encoding="utf-8") as lf:
            res = subprocess.run(cmd, env=env, stdin=subprocess.DEVNULL, stdout=lf, stderr=subprocess.STDOUT)
        if not out_shot.exists():
            print(log_file.read_text(encoding="utf-8", errors="replace"), file=sys.stderr)
            raise RuntimeError(f"Nested KWin capture failed for mode={mode} (rc={res.returncode})")


def evaluate_tahoe_card(
    shot_path: Path, ref_path: Path, gt_path: Path, x_off: int, sw: int, sh: int, out_dir: Path
) -> dict[str, Any]:
    """Compute deep pixel, optical, dispersion, rim-angular, and clarity parity metrics."""
    shot_full = Image.open(shot_path).convert("RGB")
    ours_crop = shot_full.crop((x_off, 0, x_off + sw, sh))
    ref_crop = Image.open(ref_path).convert("RGB").resize((sw, sh), Image.LANCZOS)
    gt_crop = Image.open(gt_path).convert("RGB").resize((sw, sh), Image.LANCZOS)

    ours = np.asarray(ours_crop, dtype=np.float32)
    ref = np.asarray(ref_crop, dtype=np.float32)
    gt = np.asarray(gt_crop, dtype=np.float32)

    H, W, _ = ours.shape
    scale = 2.0 / 3.0
    # Exact card geometry at 2/3 scale (matches ParityHarness x=290 -> x-x_off=184, y=154, w=700, h=491, R=101)
    x0, y0, w, h, R = 290 - x_off, 154, 700, 491, 101
    cx, cy = x0 + w * 0.5, y0 + h * 0.5

    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    d = sd_round_rect(xx, yy, cx, cy, w * 0.5, h * 0.5, R, n=2.2)
    inside = d < 0
    depth = np.maximum(-d, 0.0)

    # Mask out Apple's UI elements (scaled by 2/3 from 1600x1200 coordinates)
    ui = np.zeros((H, W), dtype=bool)
    for (r0, r1, c0, c1) in [
        (295, 545, 330, 580),   # album art
        (360, 520, 615, 1145),  # title & artist
        (645, 715, 355, 485),   # 0:22
        (645, 715, 1105, 1245), # -2:31
        (715, 795, 355, 1245),  # scrubber
        (805, 925, 355, 1245),  # controls
    ]:
        ui[int(r0 * scale) : int(r1 * scale), int(c0 * scale) : int(c1 * scale)] = True

    clean = inside & (~ui)
    clean_int = clean & (depth > 38)
    clean_bez = clean & (depth <= 38) & (depth > 2)
    shadow_band = (~inside) & (d > 2) & (d < 40) & (yy > cy)

    # 1. Core RMSE & SSIM metrics
    glass_rmse = float(np.sqrt(np.mean((ours[clean] - ref[clean]) ** 2)))
    int_rmse = float(np.sqrt(np.mean((ours[clean_int] - ref[clean_int]) ** 2)))
    bez_rmse = float(np.sqrt(np.mean((ours[clean_bez] - ref[clean_bez]) ** 2)))
    shadow_rmse = float(np.sqrt(np.mean((ours[shadow_band] - ref[shadow_band]) ** 2)))
    glass_ssim = compute_ssim(ours, ref, clean)
    int_ssim = compute_ssim(ours, ref, clean_int)
    bez_ssim = compute_ssim(ours, ref, clean_bez)

    # 2. Rim angular specular profile (72 angles around perimeter at depth 1..4px)
    lum = lambda img: 0.2126 * img[..., 0] + 0.7152 * img[..., 1] + 0.0722 * img[..., 2]
    d_lum_ours = lum(ours) - lum(gt)
    d_lum_ref = lum(ref) - lum(gt)

    rim_mask = (depth >= 0.5) & (depth <= 4.5)
    yr, xr = np.where(rim_mask)
    angs = np.arctan2(yr - cy, xr - cx)  # -pi..pi
    bins = np.linspace(-np.pi, np.pi, 73)
    prof_ours, prof_ref = [], []
    for i in range(72):
        m = (angs >= bins[i]) & (angs < bins[i + 1])
        if m.sum() > 0:
            prof_ours.append(float(np.percentile(d_lum_ours[yr[m], xr[m]], 80)))
            prof_ref.append(float(np.percentile(d_lum_ref[yr[m], xr[m]], 80)))
    prof_ours_a = np.array(prof_ours)
    prof_ref_a = np.array(prof_ref)
    rim_corr = float(np.corrcoef(prof_ours_a, prof_ref_a)[0, 1]) if len(prof_ours_a) > 5 else 0.0

    # Measure directional rim modulation: (TL+BR) vs (TR+BL) corner boost
    def corner_rim(img_dl: np.ndarray, sx: int, sy: int) -> float:
        cm = rim_mask & ((xx - cx) * sx > w * 0.25) & ((yy - cy) * sy > h * 0.25)
        return float(np.percentile(img_dl[cm], 85)) if cm.sum() > 10 else 0.0

    tl_o, br_o = corner_rim(d_lum_ours, -1, -1), corner_rim(d_lum_ours, 1, 1)
    tr_o, bl_o = corner_rim(d_lum_ours, 1, -1), corner_rim(d_lum_ours, -1, 1)
    tl_r, br_r = corner_rim(d_lum_ref, -1, -1), corner_rim(d_lum_ref, 1, 1)
    tr_r, bl_r = corner_rim(d_lum_ref, 1, -1), corner_rim(d_lum_ref, -1, 1)

    # 3. Chromatic dispersion score in top-right + bottom-right bezels (R - G deviation vs GT!)
    right_bez = clean_bez & (xx > cx + w * 0.15)
    rg_ours = (ours[..., 0] - ours[..., 1]) - (gt[..., 0] - gt[..., 1])
    rg_ref = (ref[..., 0] - ref[..., 1]) - (gt[..., 0] - gt[..., 1])
    disp_corr = float(np.corrcoef(rg_ours[right_bez], rg_ref[right_bez])[0, 1])
    disp_std_ours = float(rg_ours[right_bez].std())
    disp_std_ref = float(rg_ref[right_bez].std())

    # 4. Bezel clarity vs interior smoothness (gradient magnitude ratio)
    def grad_mag(img: np.ndarray) -> np.ndarray:
        l = lum(img)
        gx = np.abs(np.diff(l, axis=1, prepend=l[:, :1]))
        gy = np.abs(np.diff(l, axis=0, prepend=l[:1, :]))
        return np.hypot(gx, gy)

    gm_o, gm_r = grad_mag(ours), grad_mag(ref)
    clarity_o = float(gm_o[clean_bez].mean() / max(gm_o[clean_int].mean(), 1e-3))
    clarity_r = float(gm_r[clean_bez].mean() / max(gm_r[clean_int].mean(), 1e-3))

    # 5. Build diagnostic visual montage:
    # Top row: [Ours | Apple Ref | 3x Error Heatmap (masked to clean glass)]
    err_map = np.abs(ours - ref) * 3.0
    err_map[~clean] *= 0.25
    err_img = Image.fromarray(np.clip(err_map, 0, 255).astype(np.uint8))

    tw, th = 533, 400
    top = Image.new("RGB", (tw * 3, th))
    top.paste(ours_crop.resize((tw, th), Image.LANCZOS), (0, 0))
    top.paste(ref_crop.resize((tw, th), Image.LANCZOS), (tw, 0))
    top.paste(err_img.resize((tw, th), Image.LANCZOS), (tw * 2, 0))

    # Bottom row: 4 corners 1:1 (200x200 each -> scaled to 200x200 side-by-side: Ours row + Ref row)
    csize = 200
    cboxes = [
        (int(x0 - 30), int(y0 - 30), int(x0 + 170), int(y0 + 170)),             # TL
        (int(x0 + w - 170), int(y0 - 30), int(x0 + w + 30), int(y0 + 170)),     # TR
        (int(x0 - 30), int(y0 + h - 170), int(x0 + 170), int(y0 + h + 30)),     # BL
        (int(x0 + w - 170), int(y0 + h - 170), int(x0 + w + 30), int(y0 + h + 30)), # BR
    ]
    cw = (tw * 3) // 4  # 399px per corner pair!
    bot = Image.new("RGB", (tw * 3, cw))
    for i, box in enumerate(cboxes):
        # Top half = Ours, bottom half = Apple Ref (2x zoomed)
        c_o = ours_crop.crop(box).resize((cw // 2, cw // 2), Image.NEAREST)
        c_r = ref_crop.crop(box).resize((cw // 2, cw // 2), Image.NEAREST)
        # Place Ours on left half of each corner tile, Apple Ref on right half — wait: stack vertically for big 2x!
        c_o_big = ours_crop.crop(box).resize((cw - 4, cw // 2 - 2), Image.BILINEAR)
        c_r_big = ref_crop.crop(box).resize((cw - 4, cw // 2 - 2), Image.BILINEAR)
        # Actually side-by-side square: left=Ours, right=Apple (each 198x198 -> 1:1 crisp!)
        s_sq = cw // 2 - 2
        bot.paste(ours_crop.crop(box).resize((s_sq, s_sq), Image.BILINEAR), (i * cw, 0))
        bot.paste(ref_crop.crop(box).resize((s_sq, s_sq), Image.BILINEAR), (i * cw + s_sq + 2, 0))

    diag = Image.new("RGB", (tw * 3, th + cw // 2))
    diag.paste(top, (0, 0))
    diag.paste(bot.crop((0, 0, tw * 3, cw // 2)), (0, th))
    diag_path = out_dir / "case1_tahoe_diagnostic.png"
    diag.save(diag_path)

    # Also save clean full-res side-by-side (Ours | Apple) for easy eye inspection
    sbs = Image.new("RGB", (sw * 2, sh))
    sbs.paste(ours_crop, (0, 0))
    sbs.paste(ref_crop, (sw, 0))
    sbs.save(out_dir / "case1_side_by_side.png")

    metrics = {
        "glass_rmse": round(glass_rmse, 2),
        "glass_ssim": round(glass_ssim, 4),
        "interior_rmse": round(int_rmse, 2),
        "interior_ssim": round(int_ssim, 4),
        "bezel_rmse": round(bez_rmse, 2),
        "bezel_ssim": round(bez_ssim, 4),
        "rim_angular_corr": round(rim_corr, 4),
        "rim_corners_ours": {"TL": round(tl_o, 1), "BR": round(br_o, 1), "TR": round(tr_o, 1), "BL": round(bl_o, 1)},
        "rim_corners_apple": {"TL": round(tl_r, 1), "BR": round(br_r, 1), "TR": round(tr_r, 1), "BL": round(bl_r, 1)},
        "dispersion_corr": round(disp_corr, 4),
        "dispersion_std_ours": round(disp_std_ours, 2),
        "dispersion_std_apple": round(disp_std_ref, 2),
        "bezel_clarity_ours": round(clarity_o, 2),
        "bezel_clarity_apple": round(clarity_r, 2),
        "shadow_rmse": round(shadow_rmse, 2),
        "diagnostic_image": str(diag_path),
    }
    return metrics


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--build", type=Path, default=REPO_ROOT / "build/release")
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--ref", type=Path, default=DEFAULT_REF)
    ap.add_argument("--gt", type=Path, default=DEFAULT_GT)
    ap.add_argument("--cc-ref", type=Path, default=DEFAULT_CC_REF)
    ap.add_argument("--wp", type=Path, default=DEFAULT_WP)
    ap.add_argument("--cases", default="tahoe_card,control_center,optical_grid")
    args = ap.parse_args()

    args.out.mkdir(parents=True, exist_ok=True)
    cases = [c.strip() for c in args.cases.split(",") if c.strip()]
    report: dict[str, Any] = {}

    if "tahoe_card" in cases:
        bg_path = args.out / "_bg_tahoe_1280x800.png"
        x_off, sw, sh = make_1280x800_tahoe_backdrop(args.gt, bg_path)
        shot_path = args.out / "case1_tahoe_raw.png"
        run_nested_capture(args.build, "tahoe_card", bg_path, shot_path)
        m = evaluate_tahoe_card(shot_path, args.ref, args.gt, x_off, sw, sh, args.out)
        report["tahoe_card"] = m
        print("=== CASE 1: TAHOE CARD PARITY ===")
        print(f"  Glass RMSE: {m['glass_rmse']:5.2f}  SSIM: {m['glass_ssim']:.4f}")
        print(f"  Inter RMSE: {m['interior_rmse']:5.2f}  SSIM: {m['interior_ssim']:.4f}")
        print(f"  Bezel RMSE: {m['bezel_rmse']:5.2f}  SSIM: {m['bezel_ssim']:.4f}")
        print(f"  Rim angular corr: {m['rim_angular_corr']:+.4f}  Ours={m['rim_corners_ours']}  Apple={m['rim_corners_apple']}")
        print(f"  Dispersion corr:  {m['dispersion_corr']:+.4f}  std(ours)={m['dispersion_std_ours']:.1f} vs apple={m['dispersion_std_apple']:.1f}")
        print(f"  Bezel clarity:    ours={m['bezel_clarity_ours']:.2f}x vs apple={m['bezel_clarity_apple']:.2f}x")
        print(f"  Shadow RMSE:      {m['shadow_rmse']:.2f}")

    if "control_center" in cases:
        shot_cc = args.out / "case2_cc_raw.png"
        run_nested_capture(args.build, "control_center", args.wp, shot_cc)
        if args.cc_ref.exists():
            o = Image.open(shot_cc).convert("RGB").crop((440, 40, 1080, 680))
            r_full = Image.open(args.cc_ref).convert("RGB")
            rw, rh = r_full.size
            # Crop right-side Control Center from tahoe_cc.jpg
            r = r_full.crop((int(rw * 0.48), int(rh * 0.14), int(rw * 0.87), int(rh * 0.96))).resize(o.size, Image.LANCZOS)
            sbs = Image.new("RGB", (o.width * 2, o.height))
            sbs.paste(o, (0, 0))
            sbs.paste(r, (o.width, 0))
            sbs.save(args.out / "case2_cc_vs_apple.png")
            report["control_center"] = {"comparison_image": str(args.out / "case2_cc_vs_apple.png")}
            print(f"=== CASE 2: CONTROL CENTER saved -> {args.out / 'case2_cc_vs_apple.png'} ===")

    if "optical_grid" in cases:
        grid_bg = args.out / "_bg_grid_1280x800.png"
        make_optical_grid_backdrop(grid_bg)
        shot_grid = args.out / "case3_optical_grid.png"
        run_nested_capture(args.build, "optical_grid", grid_bg, shot_grid)
        report["optical_grid"] = {"image": str(shot_grid)}
        print(f"=== CASE 3: OPTICAL GRID saved -> {shot_grid} ===")

    (args.out / "metrics.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
