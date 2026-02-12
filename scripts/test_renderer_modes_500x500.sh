#!/bin/bash
# Renderer mode testing at 500×500 resolution with PLS enabled
# Orange Pi 3B (Mali-G52) + Rive Runtime

export LD_LIBRARY_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu:$LD_LIBRARY_PATH
export LIBGL_DRIVERS_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu/dri

# Fixed resolution for all tests
export RIVE_RENDER_WIDTH=500
export RIVE_RENDER_HEIGHT=500

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
                printf "%-50s %6.2f FPS (%d samples)\n", name, avg, count
            } else {
                printf "%-50s FAILED\n", name
            }
        }'
}

echo "========================================="
echo "Renderer Mode Tests @ 500×500"
echo "PLS ENABLED (Required)"
echo "Orange Pi 3B (Mali-G52) + Rive Runtime"
echo "========================================="
echo ""
echo "Test Duration: ${TEST_DURATION}s per test"
echo "Resolution: 500×500 (fixed)"
echo ""

# Baseline - PLS enabled, all defaults
echo "=== BASELINE ==="
run_test "Default (PLS enabled, all auto)" env

echo ""
echo "=== PLS CONFIGURATION VARIANTS ==="

# PLS with different fragment shader interlock settings
run_test "PLS + Fragment Interlock Disabled" env RIVE_DISABLE_FSI=1

# PLS with raster ordering disabled
run_test "PLS + Raster Ordering Disabled" env RIVE_DISABLE_RO=1

# PLS with clockwise fill override
run_test "PLS + Clockwise Fill Override" env RIVE_CLOCKWISE=1

# PLS with both FSI and RO disabled
run_test "PLS + FSI OFF + RO OFF" env RIVE_DISABLE_FSI=1 RIVE_DISABLE_RO=1

echo ""
echo "=== PLS + MSAA MODES ==="

# PLS with MSAA (may use different interlock)
run_test "PLS + MSAA 2x" env RIVE_MSAA_SAMPLES=2
run_test "PLS + MSAA 4x" env RIVE_MSAA_SAMPLES=4
run_test "PLS + MSAA 8x" env RIVE_MSAA_SAMPLES=8

echo ""
echo "=== DISPLAY MODE VARIANTS ==="

# Actual size vs stretched
run_test "PLS + Actual Size (centered)" env
run_test "PLS + Stretched to Display" env RIVE_RENDER_STRETCH=1

echo ""
echo "=== VSYNC VARIANTS ==="

# VSync on/off
run_test "PLS + VSync ON" env RIVE_VSYNC=1
run_test "PLS + VSync OFF" env RIVE_VSYNC=0

echo ""
echo "========================================="
echo "Test Complete - PLS Enabled Throughout"
echo "========================================="

