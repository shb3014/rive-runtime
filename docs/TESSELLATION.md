# Tessellation (Merged)

This document consolidates the tessellation investigation and the `RIVE_TESS_PRECISION` knob work.

If you are here for FPS: the key outcome is that, for the primary benchmark content, **tessellation was not the bottleneck**. See `docs/FPS.md` for the practical performance path.

---

## What was implemented

### `RIVE_TESS_PRECISION` environment variable

An environment variable was added to control tessellation “precision” (curve subdivision tolerance).

- **Default:** `4` (historical default; roughly “1/4 pixel precision” in the notes)
- **Range:** clamped to a reasonable range in code

**Usage:**

```bash
export RIVE_TESS_PRECISION=2   # coarser
export RIVE_TESS_PRECISION=8   # finer
```

---

## Key finding (for dress-up.riv @ 500×500)

Changing `RIVE_TESS_PRECISION` over a wide range changed triangle counts dramatically, but FPS stayed ~flat (within noise).

**Interpretation:** the workload was **fragment / PLS dominated**, not geometry dominated, on Mali‑G52 for this content/resolution.

---

## What to do instead (practical)

If your goal is higher FPS with correct rendering:

- **Reduce render resolution** (pixel count dominates) — see `docs/FPS.md`
- **Build optimizations** (PGO/LTO/flags) if needed
- **Content simplification** (paths/gradients/clips) for the biggest real gains

---

## When tessellation control *might* help

Tessellation precision can matter when you are **vertex/geometry bound**, e.g.:

- stroke-heavy content with many tiny curves
- extremely high curve complexity
- very low-end GPUs

