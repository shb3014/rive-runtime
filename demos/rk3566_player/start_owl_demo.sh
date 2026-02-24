#!/bin/bash
# Start owl tracker demo with proper Mesa environment

# Release DRM master (CRITICAL for display output!)
sudo chvt 2
sleep 1

# Set Mesa PLS environment
export LD_LIBRARY_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu:$LD_LIBRARY_PATH
export LIBGL_DRIVERS_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu/dri

# Set render resolution to 500x500 (standard benchmark)
export RIVE_RENDER_WIDTH=500
export RIVE_RENDER_HEIGHT=500

# Change to demo directory
cd ~/rive-runtime/demos/rk3566_player

# Run the demo
sudo -E ./bin/release/owl_tracker_demo ~/dress-up.riv /tmp/soulcam_scene.sock
