# Owl Eye Tracker Demo - Working Setup

## Summary

The owl's eyes in `dress-up.riv` track humans detected by SoulCam's camera AI. When a person appears in front of the camera, the owl looks in their direction (left/right/center). When no person is detected, the owl looks forward.

## Architecture

```
Camera (OV5647)
  -> SoulCam (YOLOv8n on NPU, ~22 FPS)
     -> Detection JSON via sendto() to Unix datagram socket
        -> /tmp/soulcam_scene.sock
           -> owl_tracker_demo (binds to this socket)
              -> Parses detection JSON
              -> Maps person position to look_dir: 0=center, 1=left, 2=right, 3=forward
              -> Sets look_dir input on nested "character" artboard
              -> Rive state machine transitions look animation
              -> Renders via DRM/KMS on Mali-G52 GPU
                 -> Display (1920x1280)
```

### Key discovery: Rive file structure

`dress-up.riv` has 3 artboards: **scene**, **character**, **bubble**.

- "scene" is the top-level artboard (800x800) with State Machine 1 (0 inputs)
- "character" is nested inside scene, with state machine **pose_statement** and 3 inputs:
  - `fx` - visual effects
  - `equip_change` - equipment
  - **`look_dir`** - controls which look animation plays (look_C, look_1, look_2, look_3)
- "bubble" is used for equipment UI

The `look_dir` input is accessed via the Rive NestedInput API since the nested artboard component is unnamed.

## How to Run

### SSH to device:
```bash
ssh ubuntu@192.168.1.45  # password: shb084ww
```

### Full demo (SoulCam + Rive):
```bash
~/run_owl_demo_final.sh
```

### Manual start (if SoulCam is already running with --ai):
```bash
sudo LD_LIBRARY_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu:$LD_LIBRARY_PATH \
     LIBGL_DRIVERS_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu/dri \
     ~/rive-runtime/demos/rk3566_player/bin/release/owl_tracker_demo \
     --socket /tmp/soulcam_scene.sock ~/dress-up.riv
```

### Test with fake detections (while demo is running):
```bash
python3 ~/rive-runtime/demos/rk3566_player/test_eye_tracking.py directions
python3 ~/rive-runtime/demos/rk3566_player/test_eye_tracking.py sweep
python3 ~/rive-runtime/demos/rk3566_player/test_eye_tracking.py circle
```

### Inspect the Rive file:
```bash
sudo LD_LIBRARY_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu:$LD_LIBRARY_PATH \
     LIBGL_DRIVERS_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu/dri \
     ~/rive-runtime/demos/rk3566_player/bin/release/owl_tracker_demo \
     --inspect ~/dress-up.riv
```

## Bugs Fixed (vs previous version)

1. **Socket communication**: Previous code bound to `/tmp/owl_tracker.sock` but SoulCam sends to `/tmp/soulcam_scene.sock`. Now binds directly to the SoulCam socket path.
2. **Rive input access**: Previous code looked for state machine inputs on the scene artboard (which has 0). Now accesses the nested "character" artboard's `look_dir` input via the NestedInput API.
3. **JSON parser**: Fixed to handle both compact (`"key":value`) and spaced (`"key": value`) JSON formats — SoulCam C++ produces compact, Python test scripts produce spaced.

## Performance

- SoulCam AI: ~22 FPS, ~11% CPU (YOLOv8n INT8)
- Rive rendering: ~12-13 FPS at full resolution (1920x1280)
- Latency: <100ms detection to eye movement

## Files

### Source (WSL development machine):
- `owl_tracker_demo.cpp` - Main demo application
- `build_owl_demo_simple.sh` - Build script (runs on device)
- `run_owl_demo_final.sh` - Complete demo launcher
- `test_eye_tracking.py` - Test script for fake detections

### On device (192.168.1.45):
- `~/rive-runtime/demos/rk3566_player/bin/release/owl_tracker_demo`
- `~/rive-runtime/demos/rk3566_player/test_eye_tracking.py`
- `~/run_owl_demo_final.sh`
- `~/dress-up.riv`

## Look Direction Mapping

| Camera Position | look_dir | Animation |
|----------------|----------|-----------|
| No person      | 0        | look_C    |
| Left 1/3       | 1        | look_1    |
| Right 1/3      | 2        | look_2    |
| Center 1/3     | 3        | look_3    |

## Rebuilding

```bash
ssh ubuntu@192.168.1.45
cd ~/rive-runtime/demos/rk3566_player
./build_owl_demo_simple.sh
```
