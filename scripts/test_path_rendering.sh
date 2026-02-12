#!/bin/bash
# Test script to check if Path rendering (ellipses) is now correct

DEVICE_IP="192.168.1.45"
DEVICE_USER="ubuntu"
#
# IMPORTANT: Do NOT hardcode passwords in this repo.
# Use SSH keys, or provide credentials via your SSH agent/config.

echo "=== Path Rendering Test ==="
echo ""
echo "The player is now using EXT_shader_pixel_local_storage with framebuffer fetch."
echo ""
echo "Testing on device..."
echo ""

ssh -t ${DEVICE_USER}@${DEVICE_IP} << 'ENDSSH'
cd ~/rive-runtime/demos/rk3566_player/bin/release
export LD_LIBRARY_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu:$LD_LIBRARY_PATH
export LIBGL_DRIVERS_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu/dri

echo "Running player with dress-up.riv..."
echo ""
echo "=== INSTRUCTIONS ==="
echo "1. Visually inspect the ellipses/bubbles in the scene"
echo "2. Check if they render as complete circles (not partial sectors)"
echo "3. Press Ctrl+C to stop when you've verified"
echo ""
echo "Previous issue: Ellipses showed only small angular sectors"
echo "Expected now: Complete, properly filled ellipses"
echo ""
read -p "Press Enter to start..."

./rk3566_player ~/dress-up.riv
ENDSSH

echo ""
echo "=== Test Complete ==="

