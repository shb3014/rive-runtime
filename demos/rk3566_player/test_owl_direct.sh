#!/bin/bash
# Test script to run on the device directly
# Usage: ssh to device, then run this script

sudo chvt 2
sleep 1

sudo LD_LIBRARY_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu:$LD_LIBRARY_PATH \
     LIBGL_DRIVERS_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu/dri \
     ~/rive-runtime/demos/rk3566_player/bin/release/owl_tracker_demo ~/dress-up.riv /tmp/soulcam_scene.sock
