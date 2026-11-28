// SPDX-FileCopyrightText: 2026 Kogical
// SPDX-License-Identifier: Apache-2.0
//
// Fast headless GPU (`wgpu`) visual regression & Apple Liquid Glass parity test suite.
// Runs directly on Vulkan/Metal/DX12/GL in milliseconds — zero nested compositor required!

#![cfg(feature = "wgpu")]

use std::path::{Path, PathBuf};

use image::{ImageBuffer, Rgba};
use liquidglass::wgpu_backend::WgpuRenderer;
use liquidglass::{sd_round_rect, LgMaterial, LgPointerState, LgRect, LgShape};

/// Apple reference images are not redistributable: they live in $FLUCTUS_PARITY_REFS
/// (default ~/.cache/fluctus/parity-refs) and the parity test skips itself without them.
fn parity_refs_dir() -> PathBuf {
    if let Some(dir) = std::env::var_os("FLUCTUS_PARITY_REFS") {
        return PathBuf::from(dir);
    }
    let cache = std::env::var_os("XDG_CACHE_HOME")
        .map(PathBuf::from)
        .or_else(|| std::env::var_os("HOME").map(|home| PathBuf::from(home).join(".cache")))
        .unwrap_or_default();
    cache.join("fluctus/parity-refs")
}

fn luma(px: &[u8]) -> f32 {
    0.2126 * px[0] as f32 + 0.7152 * px[1] as f32 + 0.0722 * px[2] as f32
}

fn ssim_masked(a: &[u8], b: &[u8], mask: &[bool]) -> f32 {
    let c1 = (0.01f64 * 255.0).powi(2);
    let c2 = (0.03f64 * 255.0).powi(2);
    let mut sum_ssim = 0.0f64;
    for ch in 0..3 {
        let mut n = 0.0f64;
        let mut mx = 0.0f64;
        let mut my = 0.0f64;
        for (i, &m) in mask.iter().enumerate() {
            if m {
                mx += a[i * 4 + ch] as f64;
                my += b[i * 4 + ch] as f64;
                n += 1.0;
            }
        }
        mx /= n;
        my /= n;
        let (mut vx, mut vy, mut cov) = (0.0f64, 0.0f64, 0.0f64);
        for (i, &m) in mask.iter().enumerate() {
            if m {
                let dx = a[i * 4 + ch] as f64 - mx;
                let dy = b[i * 4 + ch] as f64 - my;
                vx += dx * dx;
                vy += dy * dy;
                cov += dx * dy;
            }
        }
        vx /= n;
        vy /= n;
        cov /= n;
        sum_ssim += ((2.0 * mx * my + c1) * (2.0 * cov + c2))
            / ((mx * mx + my * my + c1) * (vx + vy + c2));
    }
    (sum_ssim / 3.0) as f32
}

#[test]
fn headless_wgpu_synthetic_optics_invariants() {
    let mut renderer = WgpuRenderer::new_headless().expect("headless wgpu adapter available");
    let (w, h) = (512u32, 384u32);
    let mut bg = vec![0u8; (w * h * 4) as usize];

    // High-contrast diagonal blue/white/gold test pattern (zero text/icons).
    for y in 0..h {
        for x in 0..w {
            let idx = ((y * w + x) * 4) as usize;
            let diag = (x as i32 - (y as i32 * 4 / 5)).rem_euclid(96);
            if diag < 28 {
                bg[idx..idx + 4].copy_from_slice(&[20, 55, 195, 255]);
            } else if diag < 34 {
                bg[idx..idx + 4].copy_from_slice(&[240, 245, 255, 255]);
            } else {
                bg[idx..idx + 4].copy_from_slice(&[85, 165, 225, 255]);
            }
        }
    }

    let shape = LgShape {
        rect: LgRect::new(80.0, 52.0, 352.0, 248.0),
        radius: 52.0,
        opacity: 1.0,
        scale: 1.0,
        material: LgMaterial::default(),
        pointer: LgPointerState::default(),
    };
    let mut out = vec![0u8; bg.len()];
    renderer
        .render_rgba8(&bg, w, h, &[shape], &mut out)
        .expect("render_rgba8");

    // 1. Far-outside pixels must remain bit-exact untouched.
    assert_eq!(&out[0..4], &bg[0..4], "far-outside corner must be unmodified");

    // 2. Bezel must produce visible chromatic dispersion (R-G divergence vs raw backdrop).
    let (cx, cy, hw, hh, r) = (80.0 + 176.0, 52.0 + 124.0, 176.0, 124.0, 52.0);
    let mut rg_sq_sum = 0.0f64;
    let mut bez_n = 0usize;
    let mut rim_tl_br = 0.0f64;
    let mut rim_tl_br_n = 0usize;
    let mut rim_tr_bl = 0.0f64;
    let mut rim_tr_bl_n = 0usize;

    for y in 0..h {
        for x in 0..w {
            let idx = ((y * w + x) * 4) as usize;
            let lp = [x as f32 + 0.5 - cx, y as f32 + 0.5 - cy];
            let d = sd_round_rect(lp, [hw, hh], r);
            if d < -2.0 && d > -24.0 {
                let drg = (out[idx] as f64 - out[idx + 1] as f64) - (bg[idx] as f64 - bg[idx + 1] as f64);
                rg_sq_sum += drg * drg;
                bez_n += 1;
            }
            if d < -0.5 && d > -3.5 {
                let dl = (luma(&out[idx..idx + 4]) - luma(&bg[idx..idx + 4])) as f64;
                if lp[0] * lp[1] > 0.0 && lp[0].abs() > hw * 0.45 && lp[1].abs() > hh * 0.45 {
                    rim_tl_br += dl;
                    rim_tl_br_n += 1;
                } else if lp[0] * lp[1] < 0.0 && lp[0].abs() > hw * 0.45 && lp[1].abs() > hh * 0.45 {
                    rim_tr_bl += dl;
                    rim_tr_bl_n += 1;
                }
            }
        }
    }
    let disp_rms = (rg_sq_sum / bez_n.max(1) as f64).sqrt();
    let avg_tl_br = rim_tl_br / rim_tl_br_n.max(1) as f64;
    let avg_tr_bl = rim_tr_bl / rim_tr_bl_n.max(1) as f64;
    assert!(disp_rms > 6.0, "expected real chromatic dispersion in bezel, got rms={disp_rms:.2}");
    assert!(
        avg_tl_br > avg_tr_bl + 8.0,
        "expected dual opposing-corner specular (TL+BR={avg_tl_br:.1} > TR+BL={avg_tr_bl:.1})"
    );
}

#[test]
fn headless_wgpu_apple_tahoe_parity() {
    let refs = parity_refs_dir();
    let ref_path = refs.join("music-card.png");
    let gt_path = refs.join("music-card-wallpaper.png");
    if !ref_path.exists() || !gt_path.exists() {
        eprintln!("Skipping Apple Tahoe parity test (reference images not cached on this host)");
        return;
    }

    let (w, h) = (1067u32, 800u32);
    let gt_img = image::open(&gt_path)
        .unwrap()
        .resize_exact(w, h, image::imageops::FilterType::Lanczos3)
        .to_rgba8();
    let ref_img = image::open(&ref_path)
        .unwrap()
        .resize_exact(w, h, image::imageops::FilterType::Lanczos3)
        .to_rgba8();

    let mut renderer = WgpuRenderer::new_headless().expect("headless wgpu adapter");
    let shape = LgShape {
        rect: LgRect::new(184.0, 154.0, 700.0, 491.0),
        radius: 101.0,
        opacity: 1.0,
        scale: 1.0,
        material: LgMaterial::default(),
        pointer: LgPointerState::default(),
    };

    let t0 = std::time::Instant::now();
    let mut out = vec![0u8; (w * h * 4) as usize];
    renderer
        .render_rgba8(gt_img.as_raw(), w, h, &[shape], &mut out)
        .expect("render_rgba8");
    let elapsed_ms = t0.elapsed().as_secs_f64() * 1000.0;

    // Mask clean glass pixels (excluding Apple's UI overlays).
    let scale = 2.0f32 / 3.0;
    let ui_boxes: [(u32, u32, u32, u32); 6] = [
        ((295.0 * scale) as u32, (545.0 * scale) as u32, (330.0 * scale) as u32, (580.0 * scale) as u32),
        ((360.0 * scale) as u32, (520.0 * scale) as u32, (615.0 * scale) as u32, (1145.0 * scale) as u32),
        ((645.0 * scale) as u32, (715.0 * scale) as u32, (355.0 * scale) as u32, (485.0 * scale) as u32),
        ((645.0 * scale) as u32, (715.0 * scale) as u32, (1105.0 * scale) as u32, (1245.0 * scale) as u32),
        ((715.0 * scale) as u32, (795.0 * scale) as u32, (355.0 * scale) as u32, (1245.0 * scale) as u32),
        ((805.0 * scale) as u32, (925.0 * scale) as u32, (355.0 * scale) as u32, (1245.0 * scale) as u32),
    ];

    let (cx, cy, hw, hh, r) = (184.0 + 350.0, 154.0 + 245.5, 350.0, 245.5, 101.0);
    let mut clean_mask = vec![false; (w * h) as usize];
    let mut sq_all = 0.0f64;
    let mut n_all = 0usize;
    let mut sq_bez = 0.0f64;
    let mut n_bez = 0usize;
    let ref_raw = ref_img.as_raw();

    for y in 0..h {
        for x in 0..w {
            let d = sd_round_rect([x as f32 - cx, y as f32 - cy], [hw, hh], r);
            if d >= 0.0 {
                continue;
            }
            let in_ui = ui_boxes.iter().any(|&(r0, r1, c0, c1)| y >= r0 && y < r1 && x >= c0 && x < c1);
            if in_ui {
                continue;
            }
            let p = (y * w + x) as usize;
            clean_mask[p] = true;
            let mut e = 0.0f64;
            for ch in 0..3 {
                let diff = out[p * 4 + ch] as f64 - ref_raw[p * 4 + ch] as f64;
                e += diff * diff;
            }
            sq_all += e / 3.0;
            n_all += 1;
            if -d <= 38.0 && -d > 2.0 {
                sq_bez += e / 3.0;
                n_bez += 1;
            }
        }
    }

    let rmse_all = (sq_all / n_all as f64).sqrt();
    let rmse_bez = (sq_bez / n_bez as f64).sqrt();
    let ssim_all = ssim_masked(&out, ref_raw, &clean_mask);

    // Save side-by-side PNG for instant visual inspection.
    let out_dir = Path::new(env!("CARGO_TARGET_TMPDIR")).join("wgpu-parity");
    let _ = std::fs::create_dir_all(&out_dir);
    let mut sbs = ImageBuffer::<Rgba<u8>, Vec<u8>>::new(w * 2, h);
    for y in 0..h {
        for x in 0..w {
            let p = ((y * w + x) * 4) as usize;
            sbs.put_pixel(x, y, Rgba([out[p], out[p + 1], out[p + 2], 255]));
            sbs.put_pixel(x + w, y, Rgba([ref_raw[p], ref_raw[p + 1], ref_raw[p + 2], 255]));
        }
    }
    let _ = sbs.save(out_dir.join("wgpu_vs_apple.png"));

    eprintln!(
        "Rust wgpu Apple Tahoe parity ({elapsed_ms:.1} ms): Glass RMSE={rmse_all:.2}, Bezel RMSE={rmse_bez:.2}, SSIM={ssim_all:.4}"
    );
    assert!(rmse_all < 30.0, "Glass RMSE regressed: {rmse_all:.2}");
    assert!(rmse_bez < 31.0, "Bezel RMSE regressed: {rmse_bez:.2}");
    assert!(ssim_all > 0.74, "Glass SSIM regressed: {ssim_all:.4}");
}
