#!/bin/bash
# Setup apitrace on Orange Pi for GL debugging

DEVICE_IP="192.168.1.45"
DEVICE_USER="ubuntu"
#
# IMPORTANT: Do NOT hardcode passwords in this repo.
# Use SSH keys, or provide credentials via your SSH agent/config.

echo "=== Setting up apitrace on Orange Pi ==="
echo ""

ssh ${DEVICE_USER}@${DEVICE_IP} << 'ENDSSH'
echo "Installing apitrace..."
sudo apt-get update
sudo apt-get install -y apitrace

echo ""
echo "Testing apitrace installation..."
which apitrace
apitrace --version

echo ""
echo "=== Capturing GL trace ==="
cd ~/rive-runtime/demos/rk3566_player/bin/release
export LD_LIBRARY_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu:$LD_LIBRARY_PATH
export LIBGL_DRIVERS_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu/dri

echo "Capturing trace (will run for 3 seconds)..."
timeout 3 apitrace trace --api=egl -o ~/ellipse_bug.trace ./rk3566_player ~/dress-up.riv

if [ -f ~/ellipse_bug.trace ]; then
    echo ""
    echo "=== Trace captured successfully ==="
    ls -lh ~/ellipse_bug.trace
    echo ""
    echo "Analyzing basic stats..."
    apitrace dump --calls ~/ellipse_bug.trace | tail -20
    echo ""
    echo "To download trace for analysis:"
    echo "  scp ${DEVICE_USER}@${DEVICE_IP}:~/ellipse_bug.trace ."
else
    echo "ERROR: Trace file not created"
fi
ENDSSH

echo ""
echo "=== Setup Complete ==="

