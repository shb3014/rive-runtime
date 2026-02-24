#!/bin/bash
# Complete owl tracker demo with SoulCam AI
# Can be run via SSH or directly on the device

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DEMO_BIN="$SCRIPT_DIR/bin/release/owl_tracker_demo"
RIV_FILE="${1:-$HOME/dress-up.riv}"
SOCKET_PATH="/tmp/soulcam_scene.sock"
SOULCAM_DIR="$HOME/SoulCam"
MODEL="YoloV8-NPU/rk3566/yolov8n.rknn"

echo "=== Owl Eye Tracker Demo ==="
echo ""

# Check prerequisites
if [ ! -f "$DEMO_BIN" ]; then
    echo "Error: owl_tracker_demo binary not found at $DEMO_BIN"
    echo "Build it first: cd $SCRIPT_DIR && ./build_owl_demo_simple.sh"
    exit 1
fi

if [ ! -f "$RIV_FILE" ]; then
    echo "Error: Rive file not found at $RIV_FILE"
    exit 1
fi

# Stop any existing instances
echo "Cleaning up previous instances..."
sudo killall -9 soulcam owl_tracker_demo 2>/dev/null || true
sleep 1

# Remove stale sockets
rm -f "$SOCKET_PATH" /tmp/owl_tracker.sock

# Start SoulCam with AI detection
echo "Starting SoulCam with AI detection..."
cd "$SOULCAM_DIR"
sudo ./build/soulcam --ai --model "$MODEL" > /tmp/soulcam_demo.log 2>&1 &
SOULCAM_PID=$!

echo "Waiting for SoulCam to initialize..."
sleep 3

if ! kill -0 $SOULCAM_PID 2>/dev/null; then
    echo "Error: SoulCam failed to start. Check /tmp/soulcam_demo.log"
    cat /tmp/soulcam_demo.log | tail -20
    exit 1
fi
echo "SoulCam started (PID: $SOULCAM_PID)"

# Switch to VT2 for DRM output
echo "Switching to VT2..."
sudo chvt 2
sleep 0.5

# Cleanup handler
cleanup() {
    echo ""
    echo "Stopping..."
    sudo kill $SOULCAM_PID 2>/dev/null || true
    sudo chvt 1
    rm -f "$SOCKET_PATH"
    echo "Done."
}
trap cleanup INT TERM EXIT

echo "Starting Owl Tracker Demo..."
echo "Rive file: $RIV_FILE"
echo "Socket: $SOCKET_PATH"
echo "Press Ctrl+C to stop"
echo ""

sudo LD_LIBRARY_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu:$LD_LIBRARY_PATH \
     LIBGL_DRIVERS_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu/dri \
     "$DEMO_BIN" --socket "$SOCKET_PATH" "$RIV_FILE"
