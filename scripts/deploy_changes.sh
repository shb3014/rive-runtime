#!/bin/bash
# Deploy modified files to Orange Pi and rebuild

set -e

DEVICE_IP="192.168.1.45"
DEVICE_USER="ubuntu"
#
# IMPORTANT: Do NOT hardcode passwords in this repo.
# Use SSH keys, or provide credentials via your SSH agent/config.

# Resolve repo root so paths work no matter where you run this from.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

echo "=== Deploying PLS Fix to Orange Pi ==="
echo ""

echo "1. Copying modified files to device..."

# Copy the modified renderer file
scp \
    "${REPO_ROOT}/renderer/src/gl/render_context_gl_impl.cpp" \
    ${DEVICE_USER}@${DEVICE_IP}:~/rive-runtime/renderer/src/gl/

# Copy the modified player file
scp \
    "${REPO_ROOT}/demos/rk3566_player/rk3566_drm_player.cpp" \
    ${DEVICE_USER}@${DEVICE_IP}:~/rive-runtime/demos/rk3566_player/

echo ""
echo "2. Files copied successfully!"
echo ""
echo "3. Building on device..."
echo ""

# SSH to device and rebuild
ssh ${DEVICE_USER}@${DEVICE_IP} << 'ENDSSH'
cd ~/rive-runtime
echo "Building Rive renderer (this will take a few minutes)..."
./build_rive.sh release --no-lto

echo ""
echo "Building rk3566_player..."
cd demos/rk3566_player
make clean
make -j4

echo ""
echo "=== Build Complete ==="
echo ""
echo "To test, run:"
echo "  cd ~/rive-runtime/demos/rk3566_player/bin/release"
echo "  export LD_LIBRARY_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu:\$LD_LIBRARY_PATH"
echo "  export LIBGL_DRIVERS_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu/dri"
echo "  ./rk3566_player ~/dress-up.riv"
ENDSSH

echo ""
echo "=== Deployment Complete ==="
echo ""
echo "Changes have been built on the device."
echo "Connect to the device to test:"
echo "  ssh ${DEVICE_USER}@${DEVICE_IP}"

