#!/bin/bash
# Run owl tracker demo with SoulCam
# This script starts both SoulCam (with AI) and the Rive owl tracker demo

set -e

echo "=== Owl Tracker Demo Runner ==="
echo ""

# Check if soulcam is already running
if pgrep -x soulcam > /dev/null; then
    echo "SoulCam is already running. Stopping it..."
    sudo pkill soulcam || true
    sleep 1
fi

# Check if owl_tracker_demo exists
if [ ! -f ~/rive-runtime/demos/rk3566_player/bin/release/owl_tracker_demo ]; then
    echo "Error: owl_tracker_demo not found!"
    echo "Please build it first:"
    echo "  cd ~/rive-runtime/demos/rk3566_player"
    echo "  ./build_owl_demo.sh"
    exit 1
fi

# Check if dress-up.riv exists
if [ ! -f ~/dress-up.riv ]; then
    echo "Error: dress-up.riv not found at ~/dress-up.riv"
    echo "Please copy the Rive file to the home directory"
    exit 1
fi

# Start SoulCam with AI in background
echo "Starting SoulCam with AI detection..."
cd ~/SoulCam
sudo ./build/soulcam --ai --model YoloV8-NPU/rk3566/yolov8n.rknn > /tmp/soulcam_demo.log 2>&1 &
SOULCAM_PID=$!

# Wait for SoulCam to initialize
echo "Waiting for SoulCam to initialize..."
sleep 3

# Check if SoulCam is running
if ! kill -0 $SOULCAM_PID 2>/dev/null; then
    echo "Error: SoulCam failed to start. Check /tmp/soulcam_demo.log"
    exit 1
fi

echo "SoulCam started (PID: $SOULCAM_PID)"
echo ""

# Set up Mesa environment for Rive
export LD_LIBRARY_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu:$LD_LIBRARY_PATH
export LIBGL_DRIVERS_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu/dri

# Start owl tracker demo
echo "Starting Owl Tracker Demo..."
echo "Press Ctrl+C to stop"
echo ""

cd ~/rive-runtime/demos/rk3566_player

# Trap Ctrl+C to clean up
trap "echo ''; echo 'Stopping...'; sudo kill $SOULCAM_PID 2>/dev/null || true; exit 0" INT TERM

# Run the demo
sudo -E ./bin/release/owl_tracker_demo ~/dress-up.riv /tmp/soulcam_scene.sock

# Cleanup
echo ""
echo "Stopping SoulCam..."
sudo kill $SOULCAM_PID 2>/dev/null || true

echo "Done."
