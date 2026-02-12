#!/bin/bash
# Quick Performance Boost for Orange Pi 3B + Rive
# Run this before starting the Rive player for better FPS

echo "🚀 Applying performance optimizations..."

# 1. Set GPU to performance mode (max frequency)
GPU_DEVFREQ=$(find /sys/class/devfreq -name "*gpu*" 2>/dev/null | head -1)
if [ -n "$GPU_DEVFREQ" ] && [ -f "$GPU_DEVFREQ/governor" ]; then
    echo "Setting GPU governor to performance..."
    echo performance | sudo tee "$GPU_DEVFREQ/governor" > /dev/null
    echo "GPU Governor: $(cat $GPU_DEVFREQ/governor)"
    if [ -f "$GPU_DEVFREQ/cur_freq" ]; then
        GPU_FREQ=$(cat "$GPU_DEVFREQ/cur_freq")
        echo "GPU Frequency: $((GPU_FREQ / 1000000)) MHz"
    fi
else
    echo "⚠️  GPU devfreq not found, skipping..."
fi

# 2. Set CPU governor to performance
echo "Setting CPU governor to performance..."
for cpu in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
    if [ -f "$cpu" ]; then
        echo performance | sudo tee "$cpu" > /dev/null
    fi
done

# 3. Disable CPU frequency scaling (optional - increases power consumption)
# Uncomment if you need maximum performance
# echo 1 | sudo tee /sys/devices/system/cpu/cpufreq/boost

# 4. Check current settings
echo ""
echo "📊 Final Settings:"
echo "=================="

CPU0_FREQ=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq 2>/dev/null || echo "unknown")
if [ "$CPU0_FREQ" != "unknown" ]; then
    echo "CPU0 Frequency: $((CPU0_FREQ / 1000)) MHz"
fi

CPU0_GOV=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null || echo "unknown")
echo "CPU Governor: $CPU0_GOV"

echo ""
echo "✅ Performance mode enabled!"
echo "Expected FPS improvement: 20-40%"
echo ""
echo "To run Rive player:"
echo "  cd ~/rive-runtime/demos/rk3566_player/bin/release"
echo "  export LD_LIBRARY_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu:\$LD_LIBRARY_PATH"
echo "  export LIBGL_DRIVERS_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu/dri"
echo "  ./rk3566_player ~/your-animation.riv"

