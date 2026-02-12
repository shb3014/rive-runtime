#!/bin/bash
# Comprehensive diagnostic for path rendering issue

DEVICE_IP="192.168.1.45"
DEVICE_USER="ubuntu"
#
# IMPORTANT: Do NOT hardcode passwords in this repo.
# Use SSH keys, or provide credentials via your SSH agent/config.

echo "=== Comprehensive Path Rendering Diagnostic ==="
echo ""

ssh ${DEVICE_USER}@${DEVICE_IP} << 'ENDSSH'
cd ~/rive-runtime/demos/rk3566_player/bin/release
export LD_LIBRARY_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu:$LD_LIBRARY_PATH
export LIBGL_DRIVERS_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu/dri

echo "========================================="
echo "DIAGNOSTIC TESTS"
echo "========================================="
echo ""

# Test 1: Default (PLS enabled)
echo "1. DEFAULT (PLS + framebuffer fetch):"
timeout 3 ./rk3566_player ~/dress-up.riv 2>&1 | grep -A 20 "Renderer Capabilities"

echo ""
echo "2. Test with PLS DISABLED:"
timeout 3 RIVE_DISABLE_PLS=1 ./rk3566_player ~/dress-up.riv 2>&1 | grep "Initializing Rive"

echo ""
echo "3. Test with Fragment Shader Interlock DISABLED:"
timeout 3 RIVE_DISABLE_FSI=1 ./rk3566_player ~/dress-up.riv 2>&1 | grep "Initializing Rive"

echo ""
echo "4. Test with MSAA forced:"
timeout 3 RIVE_MSAA_SAMPLES=4 ./rk3566_player ~/dress-up.riv 2>&1 | grep "Initializing Rive"

echo ""
echo "5. Test with clockwise fill override:"
timeout 3 RIVE_CLOCKWISE=1 ./rk3566_player ~/dress-up.riv 2>&1 | grep "Initializing Rive"

echo ""
echo "6. Test with raster ordering disabled:"
timeout 3 RIVE_DISABLE_RO=1 ./rk3566_player ~/dress-up.riv 2>&1 | grep "Initializing Rive"

echo ""
echo "========================================="
echo "MESA/GL DIAGNOSTICS"
echo "========================================="
echo ""

# Check Mesa version and features
echo "Mesa version:"
glxinfo 2>/dev/null | grep "OpenGL version" || echo "glxinfo not available"

echo ""
echo "GL Extensions related to PLS and paths:"
timeout 3 ./rk3566_player ~/dress-up.riv 2>&1 | head -50 | grep -E "shader_pixel_local|framebuffer_fetch|shader_image|color_buffer"

echo ""
echo "========================================="
echo "NEXT STEPS"
echo "========================================="
echo ""
echo "Now please run interactively and visually compare:"
echo ""
echo "TEST A - With PLS (current):"
echo "  ./rk3566_player ~/dress-up.riv"
echo ""
echo "TEST B - Without PLS:"
echo "  RIVE_DISABLE_PLS=1 ./rk3566_player ~/dress-up.riv"
echo ""
echo "If BOTH show broken ellipses → Problem is NOT PLS-specific"
echo "If ONLY PLS is broken → Panfrost PLS driver bug"
echo ""
ENDSSH

echo ""
echo "=== Diagnostic Complete ==="

