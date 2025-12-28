#!/bin/bash
# Test scenarios for the fixed Rive player on Orange Pi
# Run this script on the Orange Pi after building

set -e

PLAYER="./bin/release/rk3566_player"
RESULTS_FILE="test_results.txt"

echo "=== Rive Player Test Suite ===" | tee "$RESULTS_FILE"
echo "Date: $(date)" | tee -a "$RESULTS_FILE"
echo "" | tee -a "$RESULTS_FILE"

# Check if player exists
if [ ! -f "$PLAYER" ]; then
    echo "Error: Player not found at $PLAYER" | tee -a "$RESULTS_FILE"
    echo "Please build first: make clean && make" | tee -a "$RESULTS_FILE"
    exit 1
fi

# Function to test a .riv file
test_riv_file() {
    local file=$1
    local duration=${2:-5}  # Default 5 seconds
    
    echo "----------------------------------------" | tee -a "$RESULTS_FILE"
    echo "Testing: $file" | tee -a "$RESULTS_FILE"
    
    if [ ! -f "$file" ]; then
        echo "  ❌ File not found" | tee -a "$RESULTS_FILE"
        return 1
    fi
    
    echo "  Running for ${duration} seconds..." | tee -a "$RESULTS_FILE"
    
    # Run player with timeout
    if timeout ${duration}s sudo "$PLAYER" "$file" > /tmp/rive_test.log 2>&1; then
        echo "  ✅ Completed successfully" | tee -a "$RESULTS_FILE"
        
        # Check for FPS output
        if grep -q "FPS:" /tmp/rive_test.log; then
            fps=$(grep "FPS:" /tmp/rive_test.log | tail -1)
            echo "  $fps" | tee -a "$RESULTS_FILE"
        fi
        
        # Check for flush success
        if grep -q "after flush" /tmp/rive_test.log; then
            echo "  ✅ Flush succeeded (no crash!)" | tee -a "$RESULTS_FILE"
        fi
        
        return 0
    else
        exit_code=$?
        if [ $exit_code -eq 124 ]; then
            echo "  ✅ Timeout (expected - test passed)" | tee -a "$RESULTS_FILE"
            return 0
        else
            echo "  ❌ Crashed or failed (exit code: $exit_code)" | tee -a "$RESULTS_FILE"
            echo "  Last 10 lines of output:" | tee -a "$RESULTS_FILE"
            tail -10 /tmp/rive_test.log | sed 's/^/    /' | tee -a "$RESULTS_FILE"
            return 1
        fi
    fi
}

# Test 1: Simple test file
echo ""
echo "=== Test 1: Simple Animation ===" | tee -a "$RESULTS_FILE"
test_riv_file "test2.riv" 5

# Test 2: Complex animation
echo ""
echo "=== Test 2: Complex Animation ===" | tee -a "$RESULTS_FILE"
test_riv_file "../../dress-up.riv" 5

# Test 3: Sample files from webgpu_player
echo ""
echo "=== Test 3: Sample Animations ===" | tee -a "$RESULTS_FILE"

SAMPLE_DIR="../../renderer/webgpu_player/rivs"
if [ -d "$SAMPLE_DIR" ]; then
    # Test a few sample files
    for riv_file in "$SAMPLE_DIR/new_file.riv" \
                    "$SAMPLE_DIR/cloud_icon.riv" \
                    "$SAMPLE_DIR/stopwatch.riv"; do
        if [ -f "$riv_file" ]; then
            test_riv_file "$riv_file" 3
        fi
    done
else
    echo "  ⚠️  Sample directory not found: $SAMPLE_DIR" | tee -a "$RESULTS_FILE"
fi

# GPU Information
echo ""
echo "=== GPU Information ===" | tee -a "$RESULTS_FILE"
echo "GPU Frequency:" | tee -a "$RESULTS_FILE"
if [ -f /sys/class/devfreq/fde60000.gpu/cur_freq ]; then
    freq=$(cat /sys/class/devfreq/fde60000.gpu/cur_freq)
    echo "  Current: ${freq} Hz" | tee -a "$RESULTS_FILE"
fi

echo ""
echo "OpenGL Information:" | tee -a "$RESULTS_FILE"
if command -v glxinfo &> /dev/null; then
    glxinfo | grep -E "OpenGL (version|renderer)" | sed 's/^/  /' | tee -a "$RESULTS_FILE"
elif command -v eglinfo &> /dev/null; then
    eglinfo | grep -E "(EGL version|GL version|GL renderer)" | sed 's/^/  /' | tee -a "$RESULTS_FILE"
fi

# Memory usage
echo ""
echo "=== Memory Usage ===" | tee -a "$RESULTS_FILE"
free -h | tee -a "$RESULTS_FILE"

echo ""
echo "=== Test Suite Complete ===" | tee -a "$RESULTS_FILE"
echo "Results saved to: $RESULTS_FILE"
echo ""
echo "Summary:"
grep -E "(✅|❌)" "$RESULTS_FILE" | sort | uniq -c



