# Owl Eye Tracker Demo - Quick Start Guide

## What This Demo Does

When the camera detects a human, the owl's eyes in the Rive animation will track and follow the person's position in the frame. This creates an interactive, responsive animation that reacts to real-world camera input.

## Running the Demo

SSH into the device and run:

```bash
ssh ubuntu@192.168.1.45
~/run_owl_demo.sh
```

That's it! The script will:
1. Start SoulCam with AI detection
2. Start the Rive owl tracker demo
3. Display the animated owl on screen
4. Track any humans detected by the camera

Press Ctrl+C to stop.

## What You'll See

- The owl animation will display on the screen
- When a person appears in the camera frame, the owl's eyes will move to track them
- The eyes will follow the person as they move around
- Debug output shows detection coordinates every second

## Files Created

### On WSL/Development Machine:
- `owl_tracker_demo.cpp` - Main demo source
- `build_owl_demo_simple.sh` - Build script
- `run_owl_demo.sh` - Runner script
- `deploy_owl_demo.sh` - Deployment script
- `test_owl_demo.sh` - Test script
- `OWL_TRACKER_README.md` - Full documentation

### On Target Device (192.168.1.45):
- `~/rive-runtime/demos/rk3566_player/bin/release/owl_tracker_demo` - Compiled binary
- `~/run_owl_demo.sh` - Runner script

## Architecture Overview

```
Camera → SoulCam (YOLOv8n AI) → Unix Socket → Owl Tracker Demo → Display
         ~22 FPS detection         JSON         Rive Animation    60 FPS
```

## Technical Details

### Detection Flow:
1. Camera captures frames at 30 FPS
2. SoulCam runs YOLOv8n INT8 on NPU (~22 FPS)
3. Detections published to `/tmp/soulcam_scene.sock` as JSON
4. Owl tracker receives detections via Unix datagram socket
5. Coordinates normalized: (640×640) → (-1 to 1) → (-100 to 100)
6. Rive state machine inputs updated (eyeX, eyeY)
7. Animation rendered at 30-60 FPS

### Performance:
- SoulCam AI: ~11% CPU
- Rive rendering: ~5-10% CPU (depends on animation complexity)
- Total: ~15-20% CPU
- Latency: <100ms from detection to eye movement

### Coordinate Mapping:
- AI model space: 640×640 pixels
- Person at center (320, 320) → eyes at (0, 0)
- Person at top-left (160, 160) → eyes at (-50, -50)
- Person at bottom-right (480, 480) → eyes at (50, 50)

## Troubleshooting

### No eye movement?
1. Check SoulCam is detecting: `tail -f /tmp/soulcam_demo.log`
2. Verify detections: `python3 ~/SoulCam/scene/scene_hub.py`
3. Check Rive inputs match (eyeX/eyeY or lookX/lookY)

### Build failed?
```bash
cd ~/rive-runtime
./build.sh release
cd demos/rk3566_player
./build_owl_demo_simple.sh
```

### Permission denied?
The demo needs root for DRM/KMS access. Use `sudo -E` to preserve environment variables.

## Next Steps

To customize the demo:
1. Edit `owl_tracker_demo.cpp` to change tracking behavior
2. Modify coordinate mapping in `updateEyeTracking()`
3. Add support for multiple people tracking
4. Implement smooth interpolation for eye movement
5. Add other Rive state machine inputs (blink, mood, etc.)

## Credits

- **SoulCam**: C++ IP camera framework with RKNN AI
- **Rive**: Vector animation runtime with state machines
- **YOLOv8**: Object detection model (Rockchip NPU optimized)
- **Mesa/Panfrost**: OpenGL ES driver with PLS support
