# Debugging / Repro (Merged)

This document consolidates the debugging threads around “path/ellipse renders as partial sectors/patches” on Mali‑G52 (Panfrost), including apitrace-based verification.

**Important:** some historical notes contain conflicting intermediate hypotheses. This merged doc focuses on *stable* takeaways and points to archived sources for details.

---

## Symptom (as reported)

- Curved/path-based geometry (notably ellipses) appears as **partial angular sectors/patches** instead of complete filled shapes.

---

## What was verified during investigation (high signal)

### 1) Geometry is being submitted (apitrace evidence)

API traces include many draw calls with non-trivial counts/instances consistent with tessellation + fill passes.

### 2) Not obviously explained by viewport/scissor

Viewport/scissor checks were performed; no clear “everything is clipped away” smoking gun was found in the notes.

### 3) PLS selection/build issues were real (and fixed)

Separate from the ellipse symptom, there were confirmed issues where Linux builds did not compile/select EXT-native PLS correctly. Those were fixed (see `docs/PLS.md`).

---

## Repro / capture checklist

If you need to re-open or escalate this issue:

### A) Collect a minimal `.riv` that reproduces

- Prefer a single artboard with one or two curved paths
- Avoid unrelated assets (images/text) unless needed

### B) Capture an apitrace

- Capture a short window (a few seconds) that shows the bug on screen
- Record:
  - Mesa version (git hash if built from source)
  - GPU string + GL version
  - whether EXT-native PLS is active (capabilities)

### C) Take photos/screenshots of the display

The “partial sectors” pattern is crucial for diagnosing whether this is:
- instance/attribute corruption
- vertex transform / precision issue
- fragment coverage / winding rule bug
- driver rasterization/tiling issue

---

## Filing upstream (Mesa/Panfrost)

If the symptom persists after confirming EXT-native PLS is correctly built/selected and the trace is clean:

- File a Mesa issue with:
  - the apitrace
  - minimal `.riv`
  - photos
  - exact Mesa/Panfrost version and device info

