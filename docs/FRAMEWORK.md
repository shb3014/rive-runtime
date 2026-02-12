# Rive Runtime (this workspace) — Start Here

This repository is upstream `rive-runtime` plus a focused body of work for running and optimizing Rive vector rendering on **RK3566 / Orange Pi 3B (Mali‑G52, Panfrost)**.

This single document merges what used to be:

- framework overview
- “big plan” / workstreams
- documentation index / navigation

---

## Goal

Maximize real-world FPS for Rive vector rendering on **Orange Pi 3B (RK3566 / Mali‑G52 r1)** while keeping **rendering correctness**.

---

## Quick start (device)

### 1) Use the custom Mesa install (if applicable)

```bash
export LD_LIBRARY_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu:$LD_LIBRARY_PATH
export LIBGL_DRIVERS_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu/dri
```

### 2) Run the player with a known test asset

```bash
cd ~/rive-runtime/demos/rk3566_player

# Common benchmark resolution used in these notes:
export RIVE_RENDER_WIDTH=500
export RIVE_RENDER_HEIGHT=500

timeout 20 bin/release/rk3566_player ~/dress-up.riv 2>&1 | grep "FPS:"
```

---

## Hard constraints (validated in this workspace)

- **PLS must remain enabled** for correct rendering on the target content/device.
- **MSAA mode** and **clockwise fill** are treated as **not allowed for production** (only consider if you visually verify correctness for your content).
- Primary benchmark resolution used in many notes: **500×500**.
- DRM/KMS presentation can be effectively **60 Hz capped** when page-flip pacing is in play.

---

## Known architecture facts (high signal)

- For correctness/perf work the GL stack used here is **Mesa main (26.0.0-devel) Panfrost** installed under **`/opt/mesa-pls`**.
- Panfrost PLS enablement is tied to Mesa main work enabling `GL_EXT_shader_pixel_local_storage`.
- Vulkan (PanVK) on Mali‑G52 has historically reported **Vulkan 1.0**, while Rive’s Vulkan backend requires **Vulkan ≥ 1.1** → Vulkan backend may be blocked on this GPU/driver stack.

---

## Main performance thesis (what to optimize first)

When PLS is active and content is complex, the dominant cost is typically **fragment work** (per‑pixel PLS operations). This implies:

- The strongest “no design change” lever is **reducing pixels** (render resolution/scale) or reducing the number of pixels that execute expensive shader paths.
- Pure tessellation/triangle reduction may not help if the workload is fragment-bound.

---

## Navigation (what to read next)

### Chronological context

- `docs/PROGRESS.md` — what happened / what was learned (in order)

### Topic entry docs

- `docs/FPS.md` — performance knobs + decision guide
- `docs/PLS.md` — correctness/PLS selection and build pitfalls
- `docs/TESSELLATION.md` — tessellation control + the “not the bottleneck” result
- `docs/MESA_PANFROST.md` — Mesa/Panfrost setup notes (`/opt/mesa-pls`)
- `docs/DEBUGGING.md` — repro/capture checklist for hard rendering bugs

### Scripts & tools

- `docs/SCRIPTS.md` — 脚本索引（`scripts/`）
- `scripts/test_fps_deep_investigation.sh` — automated FPS sweep
- `scripts/apply_mesa_debug.sh` — Mesa debugging setup / logging helpers

---

## Big workstreams (plan + deliverables)

### 1) Measurement discipline (always-on)

- Standardize test commands + environment.
- Record results with consistent warmup/timing.
- Separate **vsync-capped** vs **uncapped** throughput measurements.

**Deliverables**:
- One canonical test command/script for `~/dress-up.riv` at 500×500.
- A single results table capturing FPS + config + relevant versions.

### 2) Driver/platform work (highest upside, highest uncertainty)

#### 2.1 Stay on Mesa main for Panfrost PLS

- Continue using `/opt/mesa-pls` for GLES correctness.
- Track Mesa main for fixes/optimizations relevant to:
  - PLS + framebuffer fetch
  - cache coherency / barriers
  - shader compiler improvements

#### 2.2 Vulkan viability tracking (PanVK)

- Re-check occasionally whether Mali‑G52 moves to Vulkan 1.1+.

**Deliverable**:
- A periodic “PanVK readiness” check: does it advertise Vulkan 1.1+?

### 3) Rive renderer path selection + configuration

- Keep EXT-native PLS selection as the default when supported.
- Validate interlock mode selection and impact (only if correctness is preserved).

**Deliverable**:
- A “known-good” set of env vars for correctness + best achievable FPS.

### 4) Render loop & presentation pipeline

- Confirm where any 60 Hz cap comes from (DRM page flip wait / vsync).
- Ensure benchmarking runs are not mistakenly capped when measuring raw throughput.

**Deliverable**:
- Two modes (conceptually):
  - production (tear-free, paced)
  - benchmark (uncapped if safe / or headless/offscreen)

### 5) Build/toolchain optimizations

- Prefer release builds.
- Use safe `-O3`, LTO, and CPU tuning where appropriate.
- Consider PGO if CPU submission becomes measurable.

**Deliverable**:
- Repeatable “release perf” build configuration.

### 6) Performance improvements that do NOT change design

These are allowed levers because they don’t require redesigning the Rive assets:

- Render resolution scaling / dynamic resolution.
- Optional caching of static layers into textures (if your app can separate static vs animated).
- Reducing unnecessary clears / redundant passes if safe.

**Deliverable**:
- A “knobs list” you can ship:
  - `RIVE_RENDER_WIDTH/HEIGHT` or `RIVE_RENDER_SCALE`
  - optional dynamic resolution policy
  - optional caching policy

---

## Immediate next steps

1. Lock down best correctness config at 500×500 for `~/dress-up.riv`.
2. If 60 FPS is required for this file, implement a production-friendly policy:
   - dynamic resolution targeting 60 fps, or a fixed fallback resolution.
3. Continue tracking Mesa/Panfrost improvements for PLS performance.
4. Track PanVK readiness (Vulkan ≥ 1.1) for a future Vulkan backend test.

