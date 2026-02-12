#!/bin/bash
# Script to run Rive player with PLS-enabled Mesa

export LD_LIBRARY_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu:$LD_LIBRARY_PATH
export LIBGL_DRIVERS_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu/dri

cd ~/rive-runtime/demos/rk3566_player
exec bin/release/rk3566_player "$@"

