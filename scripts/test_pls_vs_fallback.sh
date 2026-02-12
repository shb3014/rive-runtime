#!/bin/bash
# Compare rendering with PLS enabled vs disabled

DEVICE_IP="192.168.1.45"
DEVICE_USER="ubuntu"
#
# IMPORTANT: Do NOT hardcode passwords in this repo.
# Use SSH keys, or provide credentials via your SSH agent/config.

echo "=== Testing: PLS vs MSAA Fallback ==="
echo ""
echo "This will help determine if the issue is PLS-specific or affects all rendering paths."
echo ""

ssh -t ${DEVICE_USER}@${DEVICE_IP} << 'ENDSSH'
cd ~/rive-runtime/demos/rk3566_player/bin/release
export LD_LIBRARY_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu:$LD_LIBRARY_PATH
export LIBGL_DRIVERS_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu/dri

echo "========================================="
echo "TEST 1: WITH PLS (default)"
echo "========================================="
echo ""
echo "Running with EXT_shader_pixel_local_storage enabled..."
echo "Press Ctrl+C after observing the ellipses..."
echo ""
read -p "Press Enter to start Test 1..."

./rk3566_player ~/dress-up.riv

echo ""
echo ""
echo "========================================="
echo "TEST 2: WITHOUT PLS (force MSAA fallback)"
echo "========================================="
echo ""
echo "Running with PLS disabled (RIVE_DISABLE_PLS=1)..."
echo "Press Ctrl+C after observing the ellipses..."
echo ""
read -p "Press Enter to start Test 2..."

RIVE_DISABLE_PLS=1 ./rk3566_player ~/dress-up.riv

echo ""
echo ""
echo "========================================="
echo "COMPARISON RESULTS"
echo "========================================="
echo ""
echo "Please compare:"
echo "- Test 1 (with PLS): How did the ellipses look?"
echo "- Test 2 (without PLS): How did the ellipses look?"
echo ""
echo "SCENARIOS:"
echo "A) Both broken the same way → Not a PLS issue, problem elsewhere"
echo "B) PLS broken, fallback works → Panfrost PLS driver bug"
echo "C) Fallback broken, PLS works → PLS selection was the fix!"
echo "D) Both work correctly → Issue was transient/fixed"
echo ""
ENDSSH

echo ""
echo "=== Test Complete ==="

