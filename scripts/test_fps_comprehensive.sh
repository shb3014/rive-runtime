#!/bin/bash
# Comprehensive FPS optimization testing script for Orange Pi 3B
# Tests various configurations and collects performance metrics

export LD_LIBRARY_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu:$LD_LIBRARY_PATH
export LIBGL_DRIVERS_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu/dri

PLAYER="$HOME/rive-runtime/demos/rk3566_player/bin/release/rk3566_player"
RIV_FILE="$HOME/dress-up.riv"
TEST_DURATION=10

run_test() {
    local name="$1"
    shift
    echo "========================================" >&2
    echo "Testing: $name" >&2
    echo "========================================" >&2
    
    # Run test and extract average FPS from last 5 samples
    timeout $TEST_DURATION "$@" "$PLAYER" "$RIV_FILE" 2>&1 | \
        grep "FPS:" | tail -5 | \
        awk -v name="$name" '{sum+=$2; count++} END {
            if (count > 0) {
                avg = sum/count;
                printf "%-40s %6.2f FPS (%d samples)\n", name, avg, count
            } else {
                printf "%-40s FAILED\n", name
            }
        }'
}

echo "========================================="
echo "FPS Optimization Test Suite"
echo "Orange Pi 3B (Mali-G52) + Rive Runtime"
echo "========================================="
echo ""
echo "Test Duration: ${TEST_DURATION}s per test"
echo ""

# Phase 1: Resolution Tests
echo "=== PHASE 1: Resolution Scaling ==="
run_test "Baseline (Full Res 1920×1280)" env
run_test "800×600" env RIVE_RENDER_WIDTH=800 RIVE_RENDER_HEIGHT=600
run_test "640×480" env RIVE_RENDER_WIDTH=640 RIVE_RENDER_HEIGHT=480
run_test "400×400" env RIVE_RENDER_WIDTH=400 RIVE_RENDER_HEIGHT=400
run_test "320×240" env RIVE_RENDER_WIDTH=320 RIVE_RENDER_HEIGHT=240
run_test "200×200" env RIVE_RENDER_WIDTH=200 RIVE_RENDER_HEIGHT=200
echo ""

# Phase 2: PLS/Interlock Mode Tests
echo "=== PHASE 2: Renderer Mode Tests ==="
run_test "PLS Enabled (Default)" env RIVE_RENDER_WIDTH=400 RIVE_RENDER_HEIGHT=400
run_test "PLS Disabled (Fallback)" env RIVE_RENDER_WIDTH=400 RIVE_RENDER_HEIGHT=400 RIVE_DISABLE_PLS=1
run_test "Raster Ordering Disabled" env RIVE_RENDER_WIDTH=400 RIVE_RENDER_HEIGHT=400 RIVE_DISABLE_RO=1
run_test "Fragment Interlock Disabled" env RIVE_RENDER_WIDTH=400 RIVE_RENDER_HEIGHT=400 RIVE_DISABLE_FSI=1
run_test "MSAA 2x" env RIVE_RENDER_WIDTH=400 RIVE_RENDER_HEIGHT=400 RIVE_MSAA_SAMPLES=2
run_test "MSAA 4x" env RIVE_RENDER_WIDTH=400 RIVE_RENDER_HEIGHT=400 RIVE_MSAA_SAMPLES=4
run_test "Clockwise Fill Override" env RIVE_RENDER_WIDTH=400 RIVE_RENDER_HEIGHT=400 RIVE_CLOCKWISE=1
echo ""

# Phase 3: VSync Tests
echo "=== PHASE 3: VSync Tests ==="
run_test "VSync ON (400×400)" env RIVE_RENDER_WIDTH=400 RIVE_RENDER_HEIGHT=400 RIVE_VSYNC=1
run_test "VSync OFF (400×400)" env RIVE_RENDER_WIDTH=400 RIVE_RENDER_HEIGHT=400 RIVE_VSYNC=0
run_test "VSync OFF (200×200)" env RIVE_RENDER_WIDTH=200 RIVE_RENDER_HEIGHT=200 RIVE_VSYNC=0
echo ""

# Phase 4: Mesa Environment Variables
echo "=== PHASE 4: Mesa Driver Tweaks ==="
run_test "Baseline (400×400)" env RIVE_RENDER_WIDTH=400 RIVE_RENDER_HEIGHT=400
run_test "No AFBC" env RIVE_RENDER_WIDTH=400 RIVE_RENDER_HEIGHT=400 PAN_MESA_DEBUG=noafbc
run_test "Sync Disabled (DANGEROUS)" env RIVE_RENDER_WIDTH=400 RIVE_RENDER_HEIGHT=400 PAN_MESA_DEBUG=nosync
run_test "Shader Cache Enabled" env RIVE_RENDER_WIDTH=400 RIVE_RENDER_HEIGHT=400 MESA_SHADER_CACHE_DIR=/tmp/mesa_cache
echo ""

# Phase 5: Stretch vs Actual Size
echo "=== PHASE 5: Display Mode Tests ==="
run_test "400×400 Actual Size" env RIVE_RENDER_WIDTH=400 RIVE_RENDER_HEIGHT=400
run_test "400×400 Stretched" env RIVE_RENDER_WIDTH=400 RIVE_RENDER_HEIGHT=400 RIVE_RENDER_STRETCH=1
echo ""

echo "========================================="
echo "Test Complete"
echo "========================================="

