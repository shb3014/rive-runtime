# FPS / Performance (Merged)

This document consolidates the FPS/performance work previously spread across many `docs/FPS_*` files.

If you only want the practical answer: use the **Quick start** + **Decision guide** below, then only dig into the archived sources if you need deep data.

---

## Quick start (device)

### 1) Use the PLS-enabled Mesa (if applicable)

```bash
export LD_LIBRARY_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu:$LD_LIBRARY_PATH
export LIBGL_DRIVERS_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu/dri
```

### 2) Pick a render resolution

```bash
# Example: 500×500 (common benchmark)
export RIVE_RENDER_WIDTH=500
export RIVE_RENDER_HEIGHT=500

timeout 20 demos/rk3566_player/bin/release/rk3566_player ~/your-content.riv 2>&1 | grep "FPS:"
```

---

## Stable conclusions (high confidence)

### 1) **Correctness first (visual verification required)**

Some “fast” modes/configs (e.g. MSAA, clockwise fill, disabling PLS) can be **content- and driver-dependent**. Treat any FPS gain as **invalid** until you visually confirm:

- fills are correct
- colors/blends/clips are correct
- no missing geometry / artifacts

### 2) **Resolution (pixel count) is the biggest lever**

On this class of embedded GPU (Mali‑G52 / Panfrost), the workload often becomes **fragment/shader dominated** for complex vector content. Lowering render resolution typically produces the biggest speedups with minimal code risk.

### 3) **Stretching/upscaling can be expensive**

Rendering very small and then stretching to full display can cost meaningful FPS (often due to `glBlitFramebuffer()` upscale cost). If you must fill the display, it can be better to render at a higher resolution than “tiny+stretch”.

### 4) **The display pipeline can enforce a 60 Hz cap**

In DRM/KMS presentation, a page-flip wait loop can effectively cap presentation at the display refresh rate. This matters when interpreting “60 FPS” results: you may be capped by presentation rather than GPU throughput.

---

## Render resolution & display mode knobs (reference)

### Render size

- `RIVE_RENDER_WIDTH` / `RIVE_RENDER_HEIGHT`: explicit offscreen render size (highest priority)
- `RIVE_RENDER_SCALE`: percentage-based scaling (relative to display; always fills display)

Priority rule: explicit dimensions override percentage scaling.

### Display behavior (explicit dimensions)

- `RIVE_RENDER_STRETCH=0` (default): **actual size**, centered, black borders
- `RIVE_RENDER_STRETCH=1`: stretch to fill display (may blur/distort)

### Sync (measurement)

- `RIVE_VSYNC=0`: disables EGL swap interval, but **may not remove DRM page-flip pacing** if the code waits for page flip events.

---

## Decision guide (what to do)

### If you already hit 60 FPS

- ✅ You’re done for a 60 Hz display (further optimization has no visible benefit).

### If you are in the 50–59 FPS band

- **Option A (fastest to try)**: slightly reduce resolution (e.g. 450×450 instead of 500×500)
- **Option B (best quality)**: simplify content (paths, gradients, clips)
- **Option C (pragmatic)**: ship it (50+ FPS is typically smooth for UI)

### If you are <50 FPS

- **Primary**: simplify content (often the biggest real gain)
- **Secondary**: reduce resolution (e.g. 400×400 or similar)
- **Then**: consider build optimizations (PGO/LTO/flags) if needed

---

## Notes on “faster modes” (treat as experiments)

Across the historical notes, you will see claims like:
- “MSAA is faster”
- “disabling PLS is faster”
- “clockwise fill improves FPS”

These can be true for some test assets/configurations, but they have also been reported to **break correctness** for other assets. Always validate visually before adopting.

