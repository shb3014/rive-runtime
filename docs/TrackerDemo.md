# Tracker Demos — SoulCam + Rive on RK3566

Five demos that combine SoulCam's AI human detection with Rive animations.
A camera detects humans (YOLOv8n on NPU), and the Rive character's eyes/face
follow the detected person in real time.

---

## Architecture

```
┌──────────────────────────────────────────────────────┐
│                  SoulCam (AI Mode)                    │
│  Camera → ISP → YOLOv8n (RKNN NPU) → Detection JSON │
│                                          ↓           │
│                              Unix Datagram Socket     │
│                         /tmp/soulcam_scene.sock       │
└─────────────────────────────┬────────────────────────┘
                              │ JSON: { objects: [{ label, conf, box }] }
                              ↓
┌──────────────────────────────────────────────────────┐
│                  Tracker Demo                        │
│  1. Receive detection JSON                           │
│  2. Extract person bounding box center               │
│  3. Map to Rive control input (look_dir / Joystick   │
│     / pointerMove)                                   │
│  4. Render Rive animation via DRM/EGL (Mali-G52)     │
└──────────────────────────────────────────────────────┘
```

---

## Demos at a glance

| Demo | Rive file | Eye control | Tracking type | Build script |
|------|-----------|-------------|---------------|-------------|
| Owl Tracker | `dress-up.riv` | `look_dir` (SMINumber) | Discrete (3 directions) | `build_owl_demo_simple.sh` |
| Avatar Tracker | `avatar.riv` | Joystick (x/y) | Continuous | `build_avatar_demo.sh` |
| Face Tracker | `face-tracking-test.riv` | `pointerMove()` | Continuous | `build_face_demo.sh` |
| Anime Girl | `anime-girl.riv` | `pointerMove()` | Continuous | `build_avatar_demo.sh` (same binary) |
| Little Boy | `little-boy.riv` | `pointerMove()` | Continuous | `build_avatar_demo.sh` (same binary) |

All source files are under `demos/rk3566_player/`.

---

## 0. Universal Tracker (`universal_tracker_demo`)

**Source:** `universal_tracker_demo.cpp`
**Rive file:** Any `.riv` file

A single binary that auto-detects the proper control method for **any** `.riv`
file and maps SoulCam human detection to the matching Rive input.

**Auto-detection priority:**
1. **Joystick** — scans nested and main artboards for a `Joystick` object
   → continuous x/y tracking (e.g. `avatar.riv`)
2. **look_dir** — scans nested artboard state machines for `look_dir`
   SMINumber input → discrete direction tracking (e.g. `dress-up.riv`)
3. **pointerMove** — fallback; maps detection to artboard coordinates via
   `scene->pointerMove()` (e.g. `character-test.riv`, `face-tracking-test.riv`)

**CLI flags:**

| Flag | Description |
|------|-------------|
| `--resolution WxH` or `--resolution N` | Render resolution (default: 500x500) |
| `--timing` | Print per-phase frame timing in FPS log |
| `--inspect` | Dump artboard/state-machine info and exit |
| `--socket PATH` | Scene socket path (default: `/tmp/soulcam_scene.sock`) |

**Build & run:**
```bash
cd ~/rive-runtime/demos/rk3566_player
bash build_universal_demo.sh

sudo LD_LIBRARY_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu:$LD_LIBRARY_PATH \
     LIBGL_DRIVERS_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu/dri \
     ./bin/release/universal_tracker_demo --resolution 500 ~/character-test.riv
```

**Verified auto-detection results:**

| .riv file | Detected mode | Notes |
|-----------|--------------|-------|
| `avatar.riv` | Joystick | Nested artboard "Rive Avatar Rubick" |
| `dress-up.riv` | look_dir | NestedInput in "character" |
| `character-test.riv` | pointerMove | No Joystick/look_dir, fallback |
| `face-tracking-test.riv` | pointerMove | Parent-isTracking enabled |
| `anime-girl.riv` | pointerMove | General fallback |

---

## 1. Owl Tracker (`owl_tracker_demo`)

**Source:** `owl_tracker_demo.cpp`
**Rive file:** `dress-up.riv`

The owl character's eyes snap to one of three look directions based on where
the person is in the camera frame.

**How it works:**
- `dress-up.riv` has three artboards: "scene", "character", "bubble"
- "scene" nests "character", which has state machine "pose_statement"
- Input `look_dir` (number) selects which look animation plays:
  `look_C` (center), `look_1`, `look_2`, `look_3`
- Person's horizontal position in the 640x640 AI frame is mapped to a
  discrete `look_dir` value

**Build & run:**
```bash
cd ~/rive-runtime/demos/rk3566_player
bash build_owl_demo_simple.sh

sudo LD_LIBRARY_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu:$LD_LIBRARY_PATH \
     LIBGL_DRIVERS_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu/dri \
     ./bin/release/owl_tracker_demo ~/dress-up.riv /tmp/soulcam_scene.sock
```

---

## 2. Avatar Tracker (`avatar_tracker_demo`)

**Source:** `avatar_tracker_demo.cpp`
**Rive file:** `avatar.riv` (also works with `anime-girl.riv`, `little-boy.riv`)

The avatar character's eyes continuously follow the detected person using
smooth Joystick interpolation.

**How it works:**
- `avatar.riv` has two artboards: "Artboard" (wrapper) and "Rive Avatar Rubick"
- The nested artboard contains a Joystick (428x378) that drives x/y blend
  animations for head and eye movement
- Person detection coordinates are mapped to Joystick x/y range [-1, 1] with
  exponential smoothing for natural movement
- For `.riv` files without a Joystick, the demo falls back to `pointerMove()`

**CLI flags:**

| Flag | Description |
|------|-------------|
| `--resolution WxH` or `--resolution N` | Render resolution (default: 500x500) |
| `--overlay` | Use DRM overlay plane (no FBO/blit, better perf) |
| `--async-flip` | Immediate page flip, no vsync wait (may tear) |
| `--timing` | Print per-phase frame timing in FPS log |
| `--inspect` | Dump artboard/state-machine info and exit |
| `--socket PATH` | Scene socket path (default: `/tmp/soulcam_scene.sock`) |

**Build & run:**
```bash
cd ~/rive-runtime/demos/rk3566_player
bash build_avatar_demo.sh

# Standard run (500x500, overlay mode)
sudo LD_LIBRARY_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu:$LD_LIBRARY_PATH \
     LIBGL_DRIVERS_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu/dri \
     ./bin/release/avatar_tracker_demo --overlay --resolution 500 ~/avatar.riv

# 60 FPS target (overlay, 360x360)
sudo LD_LIBRARY_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu:$LD_LIBRARY_PATH \
     LIBGL_DRIVERS_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu/dri \
     ./bin/release/avatar_tracker_demo --overlay --resolution 360 ~/avatar.riv

# With timing diagnostics
sudo LD_LIBRARY_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu:$LD_LIBRARY_PATH \
     LIBGL_DRIVERS_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu/dri \
     ./bin/release/avatar_tracker_demo --overlay --timing --resolution 400 ~/avatar.riv

# Async flip for max FPS (no vsync, may tear)
sudo LD_LIBRARY_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu:$LD_LIBRARY_PATH \
     LIBGL_DRIVERS_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu/dri \
     ./bin/release/avatar_tracker_demo --overlay --async-flip --resolution 300 ~/avatar.riv
```

**Using with other `.riv` files:**
```bash
# Anime Girl
sudo ... ./bin/release/avatar_tracker_demo --overlay ~/anime-girl.riv

# Little Boy
sudo ... ./bin/release/avatar_tracker_demo --overlay ~/little-boy.riv
```

---

## 3. Face Tracker (`face_tracker_demo`)

**Source:** `face_tracker_demo.cpp`
**Rive file:** `face-tracking-test.riv`

The character's face continuously follows the detected person via `pointerMove`.

**How it works:**
- `face-tracking-test.riv` has artboard "MichiFaceTracker" (800x800) with
  state machine "FaceTracking-StateMachine"
- Inputs: `isOnGlasses` (bool), `isOnBlush` (bool), `Parent-isTracking` (bool)
- When `Parent-isTracking` is true, the face follows the pointer position
- Person detection coordinates are mapped to artboard space and fed via
  `scene->pointerMove()` with smooth interpolation

**Build & run:**
```bash
cd ~/rive-runtime/demos/rk3566_player
bash build_face_demo.sh

sudo LD_LIBRARY_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu:$LD_LIBRARY_PATH \
     LIBGL_DRIVERS_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu/dri \
     ./bin/release/face_tracker_demo --resolution 500 ~/face-tracking-test.riv
```

---

## Rendering modes

All demos support two rendering paths (see `docs/FPS.md` for benchmarks):

### FBO mode (default)
Rive renders to an offscreen FBO at the chosen resolution, which is blitted
to the center of the 1920x1280 display. Simple but wastes bandwidth on the
full-screen GBM surface.

### Overlay mode (`--overlay`)
A static black dumb buffer sits on the primary DRM plane. Rive renders
directly to a small GBM surface displayed via a DRM overlay plane, centered
on screen. Eliminates the FBO, blit, and full-screen surface. Uses atomic
modesetting (`drmModeAtomicCommit`) for non-blocking updates.

### Async flip (`--async-flip`)
Skips the vsync wait, pushing frames as fast as the GPU produces them.
Only helps at resolutions where the GPU is faster than 60 FPS (<360px).
May cause screen tearing. See `docs/FPS.md` for detailed benchmarks.

---

## Detection JSON format

SoulCam publishes detection data to `/tmp/soulcam_scene.sock` as JSON:

```json
{
  "source": "soulcam",
  "type": "detections",
  "count": 1,
  "objects": [
    {
      "cls_id": 0,
      "label": "person",
      "conf": 0.89,
      "box": { "left": 267, "top": 162, "right": 477, "bottom": 493 }
    }
  ]
}
```

The demos track the first detected person with confidence > 0.5. The AI
model runs at 640x640 input resolution on the RK3566's RKNN NPU.

---

## Coordinate mapping

```
AI frame (640x640) → Normalized [-1, 1] → Rive control input
```

| Demo | Mapping target |
|------|---------------|
| Owl | Discrete `look_dir` zones (left / center / right) |
| Avatar | Joystick x/y in [-1, 1] range |
| Face / Anime Girl / Little Boy | Artboard pixel coordinates via `pointerMove()` |

---

## Inspecting a `.riv` file

All demos support `--inspect` to dump the artboard structure without rendering:

```bash
sudo ... ./bin/release/avatar_tracker_demo --inspect ~/avatar.riv
```

This prints artboards, nested artboards, state machines, inputs, animations,
bones, and joysticks — useful for understanding how to bind a new `.riv` file.

---

## File listing

```
demos/rk3566_player/
├── universal_tracker_demo.cpp    # Universal demo — auto-detects control mode
├── avatar_tracker_demo.cpp       # Avatar/Anime/LittleBoy demo (Joystick + pointerMove)
├── face_tracker_demo.cpp         # Face tracker demo (pointerMove)
├── owl_tracker_demo.cpp          # Owl eye tracker demo (look_dir)
├── drm_egl_context.h/cpp        # DRM/KMS + EGL + overlay plane management
├── build_universal_demo.sh       # Build script (universal)
├── build_avatar_demo.sh          # Build script (avatar, also anime-girl, little-boy)
├── build_face_demo.sh            # Build script (face tracker)
├── build_owl_demo_simple.sh      # Build script (owl)
└── bin/release/                  # Compiled binaries
```

---

## Starting SoulCam for detection

The demos need SoulCam running with AI detection to produce tracking data:

```bash
cd ~/SoulCam
sudo ./build/soulcam --ai --model YoloV8-NPU/rk3566/yolov8n.rknn
```

SoulCam runs at ~22 FPS for detection (YOLOv8n INT8 on NPU), using ~11% CPU.
Detection results are published to `/tmp/soulcam_scene.sock`.

---

## Performance summary

See `docs/FPS.md` for full benchmarks. Quick reference for `avatar.riv`:

| Resolution | Overlay FPS | Overlay + async FPS |
|-----------|-------------|---------------------|
| 200x200 | 60 (vsync cap) | 108 |
| 300x300 | 60 (vsync cap) | 74 |
| 360x360 | 60 | 60 |
| 400x400 | 53 | 54 |
| 500x500 | 39 | 39 |

For 60 FPS on a 60 Hz display, use `--overlay --resolution 360` or lower.

---

## Troubleshooting

**No eye movement:** Verify SoulCam is running and detecting people. Check
that the socket path matches (default `/tmp/soulcam_scene.sock`).

**Black screen / DRM errors:** Run `sudo chvt 2` first to release the
DRM master from the login console.

**Low FPS:** Use `--overlay` mode and reduce resolution. Add `--timing` to
see which phase is the bottleneck.

**Permission errors:** All demos require `sudo` for DRM/KMS access.

**Build errors:** Ensure Rive libraries are built first:
```bash
cd ~/rive-runtime/renderer && make -C out/release -j4 rive_pls_renderer
```
