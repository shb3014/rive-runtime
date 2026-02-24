#!/bin/bash
# Deploy owl_tracker_demo to target device

TARGET_IP="${1:-192.168.1.45}"
TARGET_USER="${2:-ubuntu}"

if [ -z "$TARGET_IP" ]; then
    echo "Usage: $0 <target-ip> [username]"
    echo "Example: $0 192.168.1.45 ubuntu"
    exit 1
fi

echo "=== Deploying Owl Tracker Demo to $TARGET_USER@$TARGET_IP ==="

# Copy source files
echo "Copying source files..."
scp owl_tracker_demo.cpp drm_egl_context.cpp drm_egl_context.h \
    premake5.lua build_owl_demo.sh \
    "$TARGET_USER@$TARGET_IP:~/rive-runtime/demos/rk3566_player/"

# Copy run script
echo "Copying run script..."
scp run_owl_demo.sh "$TARGET_USER@$TARGET_IP:~/"

echo ""
echo "=== Deployment Complete ==="
echo ""
echo "To build and run on device:"
echo "  ssh $TARGET_USER@$TARGET_IP"
echo "  cd ~/rive-runtime/demos/rk3566_player"
echo "  ./build_owl_demo.sh"
echo "  ~/run_owl_demo.sh"
echo ""
