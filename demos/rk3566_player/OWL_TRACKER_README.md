# Owl Eye Tracker Demo

This demo combines SoulCam's AI human detection with Rive animation to create an interactive owl character whose eyes track detected humans in the camera frame.

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    SoulCam (AI Mode)                     │
│  Camera → ISP → AI Detection (YOLOv8n) → Scene Hub      │
│                                              ↓           │
│                                   Unix Socket (JSON)     │
└──────────────────────────────────────┬──────────────────┘
                                       │
                                       ↓
┌─────────────────────────────────────────────────────────┐
│              Owl Tracker Demo (Rive Player)              │
│  1. Receive detection JSON from scene hub               │
│  2. Extract human bounding box coordinates               │
│  3. Calculate normalized eye position (-1 to 1)          │
│  4. Update Rive state machine inputs (eyeX, eyeY)        │
│  5. Render animated owl with tracking eyes               │
└─────────────────────────────────────────────────────────┘
```

## How It Works

1. **SoulCam** runs with AI detection enabled, detecting humans in the camera frame
2. Detection data is published as JSON to `/tmp/soulcam_scene.sock`
3. **Owl Tracker Demo** receives the detection data via Unix socket
4. For each detected person:
   - Calculate the center of the bounding box
   - Normalize coordinates to [-1, 1] range
   - Scale to Rive input range (-100 to 100)
   - Update state machine number inputs (eyeX, eyeY)
5. The Rive animation responds to the input changes, making the owl's eyes track the person

## Building

On the target device:

```bash
cd ~/rive-runtime/demos/rk3566_player
./build_owl_demo_simple.sh
```

This will create `bin/release/owl_tracker_demo`.

## Running

### Option 1: Manual (two terminals)

Terminal 1 - Start SoulCam with AI:
```bash
cd ~/SoulCam
sudo ./build/soulcam --ai --model rk3566/yolov8n.rknn
```

Terminal 2 - Start Owl Tracker:
```bash
cd ~/rive-runtime/demos/rk3566_player
export LD_LIBRARY_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu:$LD_LIBRARY_PATH
export LIBGL_DRIVERS_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu/dri
sudo -E ./bin/release/owl_tracker_demo ~/dress-up.riv /tmp/soulcam_scene.sock
```

### Option 2: Automated (single command)

```bash
~/run_owl_demo.sh
```

This script:
- Stops any running SoulCam instance
- Starts SoulCam with AI detection in the background
- Starts the Owl Tracker Demo
- Handles cleanup on Ctrl+C

## Rive File Requirements

The Rive animation file (`dress-up.riv`) should have a state machine with number inputs for eye tracking. The demo looks for inputs named:

- `eyeX` or `lookX` or `trackX` or `x` (horizontal eye position)
- `eyeY` or `lookY` or `trackY` or `y` (vertical eye position)

Input range: -100 (left/top) to +100 (right/bottom)

## Detection JSON Format

The demo expects detection JSON in this format:

```json
{
  "source": "soulcam",
  "type": "detections",
  "count": 2,
  "objects": [
    {
      "cls_id": 0,
      "label": "person",
      "conf": 0.890,
      "box": {
        "left": 267,
        "top": 162,
        "right": 477,
        "bottom": 493
      }
    }
  ]
}
```

The demo tracks the first detected person with confidence > 0.5.

## Coordinate Mapping

AI model space (640×640) → Normalized space (-1 to 1) → Rive input space (-100 to 100)

Example:
- Person at center (320, 320) → (0, 0) → (0, 0)
- Person at top-left (160, 160) → (-0.5, -0.5) → (-50, -50)
- Person at bottom-right (480, 480) → (0.5, 0.5) → (50, 50)

## Troubleshooting

### No eye movement

1. Check if SoulCam is detecting humans:
   ```bash
   tail -f /tmp/soulcam.log
   ```

2. Check if detections are being published:
   ```bash
   python3 ~/SoulCam/scene/scene_hub.py
   ```

3. Check Rive state machine inputs:
   - The demo prints available inputs on startup
   - Verify input names match expected patterns

### Build errors

- Ensure Rive runtime libraries are built:
  ```bash
  cd ~/rive-runtime
  ./build.sh release
  ```

### Permission errors

- The demo needs root access for DRM/KMS:
  ```bash
  sudo -E ./bin/release/owl_tracker_demo ...
  ```

## Files

- `owl_tracker_demo.cpp` - Main demo source code
- `build_owl_demo_simple.sh` - Build script (no premake5 required)
- `run_owl_demo.sh` - Automated runner script
- `deploy_owl_demo.sh` - Deploy from WSL to device

## Performance

- SoulCam AI: ~22 FPS (YOLOv8n), ~11% CPU
- Rive rendering: 30-60 FPS depending on animation complexity
- Socket communication: <1ms latency
- Total system load: ~15-20% CPU

## Credits

- SoulCam: IP camera framework with AI detection
- Rive: Vector animation runtime
- YOLOv8: Object detection model (RKNN INT8)
