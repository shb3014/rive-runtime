# Progress — Rive Runtime on Orange Pi 3B (Mali‑G52 / Panfrost)

## Executive summary
We established a **correct-rendering** baseline using **Mesa main Panfrost + EXT-native PLS** and then iterated on FPS bottlenecks. The dominant limiter for complex content is **PLS fragment cost (per-pixel work)** and/or a **60 Hz presentation cap** when vsync/page-flip sync is active.

## Target environment
- Device: Orange Pi 3B (RK3566)
- GPU: Mali‑G52 r1 (Panfrost)
- OS: Ubuntu 24.04 aarch64
- Primary test asset: `~/dress-up.riv`
- Primary test resolution: 500×500

## What we did (chronological)

### 1) Established a working, correct PLS stack on Panfrost
- Built Mesa main (26.0.0-devel) and installed into **`/opt/mesa-pls`**.
- Verified runtime:
  - `OpenGL ES 3.1 Mesa 26.0.0-devel (git-298ad17b81)`
  - `GL_EXT_shader_pixel_local_storage: YES`
  - `GL_EXT_shader_framebuffer_fetch: YES`

Outcome:
- **Correct rendering** with PLS enabled.

### 2) Identified the 60 FPS cap in the DRM presentation path
- Found that the DRM page flip wait/swap path enforces a 60 Hz pacing.

Outcome:
- 60 FPS is a **presentation cap** when vsync/page flip sync is active.

### 3) Benchmarked `~/dress-up.riv` @ 500×500
- Baseline: ~**45–46 FPS** at 500×500 with PLS enabled.

Outcome:
- Complex content can be below 60 even at 500×500.

### 4) Tessellation quality reduction experiment
- Added runtime knob `RIVE_TESS_PRECISION` to change curve subdivision precision.
- Tested wide range (0.5 → 8).

Outcome:
- **No meaningful FPS improvement** → workload is not geometry/tessellation bound for this file.

### 5) Build/toolchain optimization experiment
- Built player with `-O3 -march=armv8-a -mtune=cortex-a55 -flto -ffast-math`.

Outcome:
- Minimal improvement at full 500×500; helps more when combined with pixel reduction.

### 6) Resolution sweep (no design changes)
- Tested multiple tile-aligned resolutions:
  - 448×448 → ~51 FPS
  - 432×432 → ~53 FPS
  - 416×416 → ~55–56 FPS
  - 400×400 → ~58 FPS
  - 384×384 → **60 FPS (cap hit)**

Outcome:
- FPS scales nearly linearly with **pixel count**; best “no-design-change” lever.

### 7) Tested GL-side tweaks (flush/clear/barrier)
- Added `RIVE_GL_FLUSH=0` toggle for `glFlush()` in GL backend.
- Added `RIVE_CLEAR_EVERY_FRAME=0` toggle for display clear path.
- Tested disabling Mali barrier in tessellation → no benefit.

Outcome:
- These toggles did **not** improve FPS for this workload (some made it worse).

### 8) Attempted Vulkan backend
- Added and built a headless Vulkan FPS runner (`vk_headless_player`) and enabled `--with_vulkan` builds.
- System Mesa PanVK could enumerate Mali-G52 only with:
  - `PAN_I_WANT_A_BROKEN_VULKAN_DRIVER=1`
  - forcing panfrost ICD
- Rive Vulkan requires Vulkan **≥ 1.1**; PanVK on Mali‑G52 reports Vulkan **1.0**.

Outcome:
- Vulkan backend is **blocked** on Mali‑G52 today due to PanVK API version.

### 9) Built Mesa main with PanVK enabled (separate install)
- Built Mesa main with `-Dvulkan-drivers=panfrost` and installed into **`/opt/mesa-pls-vk`**.

Outcome:
- PanVK still reports **Vulkan 1.0.x** for Mali‑G52 → still blocked for Rive Vulkan.

## Current best known results (dress-up.riv)
- 500×500: ~45–46 FPS (correct)
- 448×448: ~51 FPS
- 384×384: 60 FPS (cap)

## Current “known good” runtime env
### Use custom GLES+PLS Mesa
```bash
export LD_LIBRARY_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu:$LD_LIBRARY_PATH
export LIBGL_DRIVERS_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu/dri
```

### Key player knobs
- `RIVE_RENDER_WIDTH` / `RIVE_RENDER_HEIGHT`
- `RIVE_RENDER_SCALE`
- Avoid (for your content/correctness): `RIVE_MSAA_SAMPLES`, `RIVE_CLOCKWISE`

## Remaining work (no design restrictions)

### High priority
- Implement/ship a **dynamic resolution** policy targeting 60 FPS (or select a fixed “quality/perf” ladder).
- Continue tracking Mesa/Panfrost improvements that reduce PLS cost.

### Medium priority
- Add structured profiling:
  - Mesa perf counters / PAN_MESA_DEBUG
  - CPU profiling (`perf`) to see if any CPU submission is measurable

### Blocked / waiting
- Vulkan backend on Mali‑G52:
  - PanVK upstream currently reports Vulkan 1.0 for PAN_ARCH < 10.
  - Rive Vulkan backend requires ≥ 1.1.

## Status snapshot
- ✅ Correct rendering with PLS enabled
- ✅ Reproducible FPS benchmarks
- ✅ Identified tessellation is not bottleneck for `dress-up.riv`
- ✅ Found resolution scaling is primary “no-design-change” lever
- ⚠️ Vulkan backend blocked by PanVK API version
