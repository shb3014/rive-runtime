#!/bin/bash
# Deep FPS Investigation Script
# For Orange Pi 3B - 500×500 resolution, PLS enabled
# Run on target device (not cross-compile)

set -e

RESULTS_FILE="fps_deep_investigation_results.txt"
RIV_FILE="${1:-~/dress-up.riv}"
TEST_DURATION=15  # seconds per test

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "======================================"
echo "FPS Deep Investigation"
echo "======================================"
echo "Date: $(date)"
echo "Rive file: $RIV_FILE"
echo "Resolution: 500×500"
echo "PLS: Enabled (required)"
echo "Output: $RESULTS_FILE"
echo "======================================"
echo ""

# Check if binary exists
if [ ! -f "bin/release/rk3566_player" ]; then
    echo -e "${RED}Error: bin/release/rk3566_player not found${NC}"
    echo "Please build first: make config=release"
    exit 1
fi

# Check if rive file exists
if [ ! -f "$RIV_FILE" ]; then
    echo -e "${RED}Error: Rive file not found: $RIV_FILE${NC}"
    exit 1
fi

# Setup environment
export LD_LIBRARY_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu:$LD_LIBRARY_PATH
export LIBGL_DRIVERS_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu/dri

# Initialize results file
echo "FPS Deep Investigation Results" > "$RESULTS_FILE"
echo "Date: $(date)" >> "$RESULTS_FILE"
echo "Rive File: $RIV_FILE" >> "$RESULTS_FILE"
echo "Resolution: 500×500 (PLS enabled)" >> "$RESULTS_FILE"
echo "Test Duration: ${TEST_DURATION}s per test" >> "$RESULTS_FILE"
echo "========================================" >> "$RESULTS_FILE"
echo "" >> "$RESULTS_FILE"

# Helper function to run test
run_test() {
    local test_name="$1"
    shift
    local env_vars="$@"
    
    echo -e "${YELLOW}Testing: $test_name${NC}"
    echo "=== $test_name ===" >> "$RESULTS_FILE"
    echo "Environment: $env_vars" >> "$RESULTS_FILE"
    
    # Run test and capture FPS lines
    local output=$(timeout $TEST_DURATION $env_vars bin/release/rk3566_player "$RIV_FILE" 2>&1 | grep "FPS:" | tail -10)
    
    echo "$output" >> "$RESULTS_FILE"
    
    # Calculate average FPS
    if [ -n "$output" ]; then
        local avg_fps=$(echo "$output" | awk '{sum+=$2; count++} END {if(count>0) printf "%.2f", sum/count; else print "N/A"}')
        echo "Average FPS: $avg_fps" >> "$RESULTS_FILE"
        echo -e "${GREEN}  → Average FPS: $avg_fps${NC}"
    else
        echo "No FPS data captured" >> "$RESULTS_FILE"
        echo -e "${RED}  → No FPS data${NC}"
    fi
    
    echo "" >> "$RESULTS_FILE"
    sleep 2  # Cool down between tests
}

# ============================================
# Phase 1: Baseline & VSync Tests
# ============================================
echo ""
echo "========================================" | tee -a "$RESULTS_FILE"
echo "PHASE 1: Baseline & VSync Tests" | tee -a "$RESULTS_FILE"
echo "========================================" | tee -a "$RESULTS_FILE"
echo ""

run_test "Baseline (500×500, PLS enabled, VSync ON)" \
    "RIVE_RENDER_WIDTH=500 RIVE_RENDER_HEIGHT=500"

run_test "VSync OFF (measure uncapped FPS)" \
    "RIVE_RENDER_WIDTH=500 RIVE_RENDER_HEIGHT=500 RIVE_VSYNC=0"

run_test "VSync OFF + Interlock tweaks" \
    "RIVE_RENDER_WIDTH=500 RIVE_RENDER_HEIGHT=500 RIVE_VSYNC=0 RIVE_DISABLE_RO=1 RIVE_DISABLE_FSI=1"

# ============================================
# Phase 2: Resolution Sweet Spot
# ============================================
echo ""
echo "========================================" | tee -a "$RESULTS_FILE"
echo "PHASE 2: Resolution Sweet Spot Testing" | tee -a "$RESULTS_FILE"
echo "========================================" | tee -a "$RESULTS_FILE"
echo ""

for size in 400 425 450 475 500 525 550 575 600; do
    run_test "Resolution ${size}×${size} (PLS, VSync OFF)" \
        "RIVE_RENDER_WIDTH=$size RIVE_RENDER_HEIGHT=$size RIVE_VSYNC=0"
done

# ============================================
# Phase 3: Interlock Mode Variations
# ============================================
echo ""
echo "========================================" | tee -a "$RESULTS_FILE"
echo "PHASE 3: PLS Interlock Variations" | tee -a "$RESULTS_FILE"
echo "========================================" | tee -a "$RESULTS_FILE"
echo ""

run_test "PLS default (baseline)" \
    "RIVE_RENDER_WIDTH=500 RIVE_RENDER_HEIGHT=500 RIVE_VSYNC=0"

run_test "PLS + Raster Ordering OFF" \
    "RIVE_RENDER_WIDTH=500 RIVE_RENDER_HEIGHT=500 RIVE_VSYNC=0 RIVE_DISABLE_RO=1"

run_test "PLS + Fragment Interlock OFF" \
    "RIVE_RENDER_WIDTH=500 RIVE_RENDER_HEIGHT=500 RIVE_VSYNC=0 RIVE_DISABLE_FSI=1"

run_test "PLS + Both OFF (RO + FSI)" \
    "RIVE_RENDER_WIDTH=500 RIVE_RENDER_HEIGHT=500 RIVE_VSYNC=0 RIVE_DISABLE_RO=1 RIVE_DISABLE_FSI=1"

# ============================================
# Phase 4: Display Mode Tests
# ============================================
echo ""
echo "========================================" | tee -a "$RESULTS_FILE"
echo "PHASE 4: Display Mode Tests" | tee -a "$RESULTS_FILE"
echo "========================================" | tee -a "$RESULTS_FILE"
echo ""

run_test "Actual size (centered)" \
    "RIVE_RENDER_WIDTH=500 RIVE_RENDER_HEIGHT=500 RIVE_VSYNC=0"

run_test "Stretched to display" \
    "RIVE_RENDER_WIDTH=500 RIVE_RENDER_HEIGHT=500 RIVE_RENDER_STRETCH=1 RIVE_VSYNC=0"

# ============================================
# Phase 5: System Information
# ============================================
echo ""
echo "========================================" | tee -a "$RESULTS_FILE"
echo "PHASE 5: System Information" | tee -a "$RESULTS_FILE"
echo "========================================" | tee -a "$RESULTS_FILE"
echo ""

echo "=== GPU Frequency ===" >> "$RESULTS_FILE"
cat /sys/class/devfreq/fde60000.gpu/cur_freq >> "$RESULTS_FILE" 2>&1 || echo "N/A" >> "$RESULTS_FILE"
cat /sys/class/devfreq/fde60000.gpu/available_frequencies >> "$RESULTS_FILE" 2>&1 || echo "N/A" >> "$RESULTS_FILE"
echo "" >> "$RESULTS_FILE"

echo "=== GPU Governor ===" >> "$RESULTS_FILE"
cat /sys/class/devfreq/fde60000.gpu/governor >> "$RESULTS_FILE" 2>&1 || echo "N/A" >> "$RESULTS_FILE"
echo "" >> "$RESULTS_FILE"

echo "=== CPU Frequencies ===" >> "$RESULTS_FILE"
for cpu in /sys/devices/system/cpu/cpu[0-9]*; do
    if [ -f "$cpu/cpufreq/scaling_cur_freq" ]; then
        echo "$(basename $cpu): $(cat $cpu/cpufreq/scaling_cur_freq) kHz" >> "$RESULTS_FILE"
    fi
done
echo "" >> "$RESULTS_FILE"

echo "=== CPU Governor ===" >> "$RESULTS_FILE"
cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor >> "$RESULTS_FILE" 2>&1 || echo "N/A" >> "$RESULTS_FILE"
echo "" >> "$RESULTS_FILE"

echo "=== Temperature ===" >> "$RESULTS_FILE"
for thermal in /sys/class/thermal/thermal_zone*/temp; do
    if [ -f "$thermal" ]; then
        temp=$(cat "$thermal")
        temp_c=$((temp / 1000))
        echo "$(dirname $thermal): ${temp_c}°C" >> "$RESULTS_FILE"
    fi
done
echo "" >> "$RESULTS_FILE"

echo "=== Memory Info ===" >> "$RESULTS_FILE"
free -h >> "$RESULTS_FILE"
echo "" >> "$RESULTS_FILE"

echo "=== GL Info ===" >> "$RESULTS_FILE"
echo "Mesa path: $LIBGL_DRIVERS_PATH" >> "$RESULTS_FILE"
echo "Library path: $LD_LIBRARY_PATH" >> "$RESULTS_FILE"
echo "" >> "$RESULTS_FILE"

# ============================================
# Summary & Analysis
# ============================================
echo ""
echo "========================================" | tee -a "$RESULTS_FILE"
echo "SUMMARY" | tee -a "$RESULTS_FILE"
echo "========================================" | tee -a "$RESULTS_FILE"
echo ""

echo "Testing complete! Results saved to: $RESULTS_FILE"
echo ""
echo "Key questions answered:"
echo "1. Uncapped FPS (vsync=0): Check 'VSync OFF' test"
echo "2. Optimal resolution: Compare 400-600 range"
echo "3. Interlock impact: Compare PLS variations"
echo "4. Display mode cost: Actual size vs stretched"
echo ""
echo "Next steps:"
echo "- Review $RESULTS_FILE for detailed data"
echo "- If uncapped FPS > 60: vsync is limiting factor"
echo "- If uncapped FPS ≈ 45-50: GPU/shader bottleneck"
echo "- Consider build optimizations (PGO, LTO)"
echo "- Consider tessellation quality reduction"
echo ""

# Generate quick summary
echo "Quick Summary:" | tee -a "$RESULTS_FILE"
echo "-------------" | tee -a "$RESULTS_FILE"
baseline_fps=$(grep -A1 "Baseline (500×500" "$RESULTS_FILE" | grep "Average FPS:" | awk '{print $3}')
uncapped_fps=$(grep -A1 "VSync OFF (measure uncapped" "$RESULTS_FILE" | grep "Average FPS:" | awk '{print $3}')

if [ -n "$baseline_fps" ] && [ -n "$uncapped_fps" ]; then
    echo "Baseline (vsync on):  $baseline_fps FPS" | tee -a "$RESULTS_FILE"
    echo "Uncapped (vsync off): $uncapped_fps FPS" | tee -a "$RESULTS_FILE"
    
    # Calculate percentage difference
    if command -v bc &> /dev/null; then
        diff=$(echo "scale=2; (($uncapped_fps - $baseline_fps) / $baseline_fps) * 100" | bc)
        echo "Improvement: ${diff}% faster without vsync" | tee -a "$RESULTS_FILE"
        
        # Interpret results
        echo "" | tee -a "$RESULTS_FILE"
        echo "Interpretation:" | tee -a "$RESULTS_FILE"
        if (( $(echo "$uncapped_fps > 60" | bc -l) )); then
            echo "✅ GPU has headroom! VSync is the bottleneck." | tee -a "$RESULTS_FILE"
            echo "   → Enable vsync=0 for production OR optimize to hit 60 FPS capped" | tee -a "$RESULTS_FILE"
        elif (( $(echo "$uncapped_fps < 50" | bc -l) )); then
            echo "⚠️  GPU/shader bottleneck. Need deeper optimizations:" | tee -a "$RESULTS_FILE"
            echo "   → Tessellation quality reduction" | tee -a "$RESULTS_FILE"
            echo "   → Build optimizations (PGO, LTO, fast-math)" | tee -a "$RESULTS_FILE"
            echo "   → Content simplification" | tee -a "$RESULTS_FILE"
        else
            echo "ℹ️  Moderate headroom (50-60 FPS uncapped)" | tee -a "$RESULTS_FILE"
            echo "   → Minor optimizations may reach 60 FPS cap" | tee -a "$RESULTS_FILE"
        fi
    fi
fi

echo ""
echo -e "${GREEN}Investigation complete!${NC}"
echo "Results: $RESULTS_FILE"

