# PLS / Rendering Correctness (Merged)

This document consolidates the Pixel Local Storage (PLS) work for **Rive on Mali‑G52 (Panfrost)**: what PLS is, how Rive selects it, and the fixes required for Linux/GLES to use **EXT-native PLS** correctly.

If you are here for performance, also read `docs/FPS.md`.

---

## What PLS is (in Rive)

Rive’s GPU path renderer relies on **per‑pixel coverage accumulation** and then a resolve step to produce correct fills, blends, and clipping. On GLES, the preferred implementation is **EXT-native PLS** when available.

---

## What you need on Panfrost (Mali‑G52)

### Mesa side (driver capabilities)

For EXT-native PLS, you need:

- `GL_EXT_shader_pixel_local_storage`
- plus framebuffer fetch: `GL_EXT_shader_framebuffer_fetch` (or `GL_ARM_shader_framebuffer_fetch`)

### Rive side (two common failure modes)

To actually *use* EXT-native PLS on Linux/GLES, two things must be true:

1) **Build includes** the implementation (`pls_impl_ext_native.cpp` + GLES extension loader) for Linux builds  
2) **Runtime selection** chooses EXT-native PLS on non-Android GLES when the extensions are present

Historically, each of these was a separate foot-gun.

---

## Fix #1 — Build system: compile EXT-native PLS on Linux

**Symptom:** fills missing / only strokes, or renderer silently falls back to MSAA interlock mode.

**Root cause:** `renderer/premake5_pls_renderer.lua` included `pls_impl_ext_native.cpp` only under `system:android`, so Linux builds never compiled the EXT-native implementation even if the driver supported it.

**Fix:** include these sources under the Linux (and other desktop OS) filters:

- `src/gl/pls_impl_ext_native.cpp`
- `src/gl/load_gles_extensions.cpp`

---

## Fix #2 — Runtime selection: enable EXT-native PLS on non-Android GLES

**Symptom:** driver prints EXT PLS support, but Rive still does not select `MakePLSImplEXTNative(...)` on Linux.

**Root cause:** selection logic only considered EXT-native PLS under `RIVE_ANDROID`, and on non-Android GLES it could fall through to “no PLS impl” even when EXT PLS + framebuffer fetch existed.

**Fix:** in `renderer/src/gl/render_context_gl_impl.cpp`, for the non-Android GLES branch:

- if `EXT_shader_pixel_local_storage` and framebuffer fetch are present, choose EXT-native PLS (`MakePLSImplEXTNative`)

---

## Quick verification checklist (device)

1) Run with the intended Mesa install (example prefix):

```bash
export LD_LIBRARY_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu:$LD_LIBRARY_PATH
export LIBGL_DRIVERS_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu/dri
```

2) Run the player and confirm the reported capabilities include:

- `EXT_shader_pixel_local_storage: YES`
- framebuffer fetch (EXT or ARM): `YES`

3) **Visually verify** fills, clipping, and blends for your real `.riv` content.

---

## Performance notes (high signal)

- EXT-native PLS tends to be **fragment-work heavy**. The biggest safe lever is usually **render resolution** (pixel count). See `docs/FPS.md`.
- Some “faster” modes like MSAA/clockwise/disable‑PLS can appear attractive in benchmarks but may **break correctness** for your content. Treat them as experiments only.

