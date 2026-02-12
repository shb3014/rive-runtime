# Mesa / Panfrost (PLS) on Mali‑G52 (Merged)

This document consolidates the Mesa/Panfrost work needed to run Rive with EXT-native PLS on Mali‑G52, including the “custom Mesa in `/opt/mesa-pls`” approach.

---

## Goal

- Use Mesa main (Panfrost) with `GL_EXT_shader_pixel_local_storage` enabled
- Keep it **separate** from system Mesa
- Run the Rive player against the custom install via env vars

---

## Quick verification (high signal)

When running with the custom install, you should see something like:

- GL: `OpenGL ES 3.1 Mesa 26.0.0-devel (...)`
- Renderer: `Mali-G52 r1 (Panfrost)`
- Extensions: `EXT_shader_pixel_local_storage: YES` (and framebuffer fetch YES)

### Runtime env vars

```bash
export LD_LIBRARY_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu:$LD_LIBRARY_PATH
export LIBGL_DRIVERS_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu/dri
```

---

## Why build Mesa from source?

Ubuntu packages (and typical ARM snapshots) may not include the Panfrost PLS enablement you need. The historical notes reference Mesa main commits enabling PLS for Panfrost.

---

## Build/install approach (conceptual)

The documented approach was:

- build Mesa main with Panfrost Gallium driver and EGL/GLES enabled
- install under `/opt/mesa-pls` (often via `DESTDIR` install to avoid Meson install state issues)
- use the env vars above to run apps against the custom libraries/drivers

