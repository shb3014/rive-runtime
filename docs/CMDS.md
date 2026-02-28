# Commands Reference

Target device: `192.168.1.45` (Orange Pi 3B, RK3566, Mali-G52 r1 Panfrost)
SSH: `ssh ubuntu@192.168.1.45` (password: `shb084ww`)

---

## 1. Device Setup (one-time per boot)

### Set GPU governor to performance
```bash
echo shb084ww | sudo -S sh -c 'echo performance > /sys/devices/platform/fde60000.gpu/devfreq/fde60000.gpu/governor'
```

### Verify GPU frequency (should be 800MHz)
```bash
cat /sys/devices/platform/fde60000.gpu/devfreq/fde60000.gpu/cur_freq
# Expected: 800000000
```

### Release DRM master for the player (required before running)
```bash
sudo chvt 2
```

### Restore VT after testing
```bash
sudo chvt 1
```

---

## 2. Build on Device (native, no cross-compile)

All builds run on the device itself. SSH in first:
```bash
sshpass -p 'shb084ww' ssh ubuntu@192.168.1.45
```

### Premake5 binary (native aarch64)
```bash
PREMAKE5=~/rive-runtime/build/dependencies/premake-core/bin/v5.0.0-beta7_release/premake5
```

### Build renderer library (incremental)
```bash
cd ~/rive-runtime/renderer
make -C out/release -j4 rive_pls_renderer
```

### Build decoder + image libraries (if missing)
```bash
cd ~/rive-runtime/renderer
make -C out/release -j4 rive_decoders libpng zlib libjpeg libwebp
```

### Regenerate player Makefiles (after premake5.lua changes)
```bash
cd ~/rive-runtime/demos/rk3566_player
$PREMAKE5 gmake2
```

### Build player
```bash
cd ~/rive-runtime/demos/rk3566_player
make config=release -j4
```

---

## 3. Sync Code from Host to Device

Run from the host machine (WSL2). Syncs only the files that matter for rebuild.

### Sync renderer GL source
```bash
cd ~/embeddedProjects/rive-runtime
sshpass -p 'shb084ww' rsync -avz --delete \
  renderer/src/gl/ \
  ubuntu@192.168.1.45:~/rive-runtime/renderer/src/gl/
```

### Sync renderer headers
```bash
sshpass -p 'shb084ww' rsync -avz --delete \
  renderer/include/ \
  ubuntu@192.168.1.45:~/rive-runtime/renderer/include/
```

### Sync generated shader headers
```bash
sshpass -p 'shb084ww' rsync -avz --delete \
  renderer/out/release/include/generated/shaders/ \
  ubuntu@192.168.1.45:~/rive-runtime/renderer/out/release/include/generated/shaders/
```

### Sync PLS shader source + minifier
```bash
sshpass -p 'shb084ww' rsync -avz \
  renderer/src/shaders/pls_load_store_ext.glsl \
  renderer/src/shaders/minify.py \
  ubuntu@192.168.1.45:~/rive-runtime/renderer/src/shaders/
```

### Sync DRM player source
```bash
sshpass -p 'shb084ww' rsync -avz --delete \
  demos/rk3566_player/ \
  --exclude='bin/' --exclude='obj/' --exclude='.gitignore' \
  ubuntu@192.168.1.45:~/rive-runtime/demos/rk3566_player/
```

---

## 4. Run the Generic Player

### Standard run (500x500, PLS, Panfrost)
```bash
sudo chvt 2
sudo bash -c '
  export LD_LIBRARY_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu:$LD_LIBRARY_PATH
  export LIBGL_DRIVERS_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu/dri
  export RIVE_RENDER_WIDTH=500
  export RIVE_RENDER_HEIGHT=500
  ~/rive-runtime/demos/rk3566_player/bin/release/rk3566_player ~/dress-up.riv
'
```

### Timed benchmark (20 seconds)
```bash
sudo chvt 2
sudo bash -c '
  export LD_LIBRARY_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu:$LD_LIBRARY_PATH
  export LIBGL_DRIVERS_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu/dri
  export RIVE_RENDER_WIDTH=500
  export RIVE_RENDER_HEIGHT=500
  timeout 20 ~/rive-runtime/demos/rk3566_player/bin/release/rk3566_player ~/dress-up.riv 2>&1
' | grep "FPS:"
```

### One-liner from host (SSH, benchmark)
```bash
sshpass -p 'shb084ww' ssh ubuntu@192.168.1.45 \
  "echo shb084ww | sudo -S chvt 2; sleep 1; echo shb084ww | sudo -S bash -c '
    export LD_LIBRARY_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu:\$LD_LIBRARY_PATH
    export LIBGL_DRIVERS_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu/dri
    export RIVE_RENDER_WIDTH=500
    export RIVE_RENDER_HEIGHT=500
    timeout 20 ~/rive-runtime/demos/rk3566_player/bin/release/rk3566_player ~/dress-up.riv 2>&1
  ' | grep 'FPS:'"
```

### Stop the player (from host)
```bash
sshpass -p 'shb084ww' ssh ubuntu@192.168.1.45 \
  "echo shb084ww | sudo -S killall rk3566_player; echo shb084ww | sudo -S chvt 1"
```

---

## 5. Tracker Demos (SoulCam + Rive)

See `docs/TrackerDemo.md` for full details on each demo.

### Common env setup (add to every sudo command)
```bash
export LD_LIBRARY_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu:$LD_LIBRARY_PATH
export LIBGL_DRIVERS_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu/dri
```

### Build tracker demos (on device)
```bash
cd ~/rive-runtime/demos/rk3566_player
bash build_universal_demo.sh    # universal (auto-detects any .riv)
bash build_avatar_demo.sh       # avatar / anime-girl / little-boy
bash build_face_demo.sh         # face tracker
bash build_owl_demo_simple.sh   # owl
```

### Start SoulCam (needed for all tracker demos)
```bash
cd ~/SoulCam
sudo ./build/soulcam --ai --model YoloV8-NPU/rk3566/yolov8n.rknn
```

### Run Avatar Tracker (overlay, 360x360 for 60 FPS)
```bash
sudo LD_LIBRARY_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu:$LD_LIBRARY_PATH \
     LIBGL_DRIVERS_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu/dri \
     ~/rive-runtime/demos/rk3566_player/bin/release/avatar_tracker_demo \
     --overlay --resolution 360 ~/avatar.riv
```

### Run Avatar Tracker with timing diagnostics
```bash
sudo LD_LIBRARY_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu:$LD_LIBRARY_PATH \
     LIBGL_DRIVERS_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu/dri \
     ~/rive-runtime/demos/rk3566_player/bin/release/avatar_tracker_demo \
     --overlay --timing --resolution 500 ~/avatar.riv
```

### Run Avatar Tracker with async flip (no vsync, may tear)
```bash
sudo LD_LIBRARY_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu:$LD_LIBRARY_PATH \
     LIBGL_DRIVERS_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu/dri \
     ~/rive-runtime/demos/rk3566_player/bin/release/avatar_tracker_demo \
     --overlay --async-flip --timing --resolution 300 ~/avatar.riv
```

### Run with other .riv files (same binary)
```bash
# Anime Girl
sudo ... avatar_tracker_demo --overlay ~/anime-girl.riv

# Little Boy
sudo ... avatar_tracker_demo --overlay ~/little-boy.riv
```

### Run Face Tracker
```bash
sudo LD_LIBRARY_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu:$LD_LIBRARY_PATH \
     LIBGL_DRIVERS_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu/dri \
     ~/rive-runtime/demos/rk3566_player/bin/release/face_tracker_demo \
     --resolution 500 ~/face-tracking-test.riv
```

### Run Owl Tracker
```bash
sudo LD_LIBRARY_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu:$LD_LIBRARY_PATH \
     LIBGL_DRIVERS_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu/dri \
     ~/rive-runtime/demos/rk3566_player/bin/release/owl_tracker_demo \
     ~/dress-up.riv /tmp/soulcam_scene.sock
```

### Run Universal Tracker (auto-detects control mode for any .riv)
```bash
sudo LD_LIBRARY_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu:$LD_LIBRARY_PATH \
     LIBGL_DRIVERS_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu/dri \
     ~/rive-runtime/demos/rk3566_player/bin/release/universal_tracker_demo \
     --resolution 500 ~/character-test.riv
```

### Inspect a .riv file (dump structure, no rendering)
```bash
sudo ... universal_tracker_demo --inspect ~/character-test.riv
```

### Stop a running demo (from host)
```bash
sshpass -p 'shb084ww' ssh ubuntu@192.168.1.45 \
  "sudo killall avatar_tracker_demo face_tracker_demo owl_tracker_demo 2>/dev/null"
```

### Deploy updated source from host to device
```bash
cd ~/embeddedProjects/SoulCam/rive-runtime/demos/rk3566_player
sshpass -p 'shb084ww' scp -o StrictHostKeyChecking=no \
  avatar_tracker_demo.cpp drm_egl_context.h drm_egl_context.cpp \
  ubuntu@192.168.1.45:~/rive-runtime/demos/rk3566_player/
```

### Force full rebuild (after drm_egl_context changes)
```bash
sshpass -p 'shb084ww' ssh ubuntu@192.168.1.45 \
  'rm -f ~/rive-runtime/demos/rk3566_player/bin/release/drm_egl_context.o && \
   cd ~/rive-runtime/demos/rk3566_player && bash build_avatar_demo.sh'
```

---

## 6. Render Resolution Knobs (generic player)

| Env var | Description |
|---------|-------------|
| `RIVE_RENDER_WIDTH` / `RIVE_RENDER_HEIGHT` | Explicit offscreen render size (highest priority) |
| `RIVE_RENDER_SCALE` | Percentage-based scaling relative to display |
| `RIVE_RENDER_STRETCH=1` | Stretch to fill display (default 0 = centered, black borders) |
| `RIVE_VSYNC=0` | Disable EGL swap interval |

### Resolution sweep example
```bash
for SIZE in 384 432 500 600 700; do
  echo "=== ${SIZE}x${SIZE} ==="
  sudo bash -c "
    export LD_LIBRARY_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu:\$LD_LIBRARY_PATH
    export LIBGL_DRIVERS_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu/dri
    export RIVE_RENDER_WIDTH=$SIZE
    export RIVE_RENDER_HEIGHT=$SIZE
    timeout 10 ~/rive-runtime/demos/rk3566_player/bin/release/rk3566_player ~/dress-up.riv 2>&1
  " | grep "FPS:" | tail -3
done
```

---

## 7. Diagnostics

### Check which DRM card is which
```bash
for i in 0 1 2; do
  echo -n "card$i: "
  sudo cat /sys/class/drm/card$i/device/uevent 2>/dev/null | grep DRIVER
done
# Expected: card0=rockchip-drm (display), card2=panfrost (GPU)
```

### Verify Mesa PLS install
```bash
LD_LIBRARY_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu \
LIBGL_DRIVERS_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu/dri \
  eglinfo 2>&1 | head -20
```

### Check GPU clock (SCMI, real frequency)
```bash
sudo cat /sys/kernel/debug/clk/clk_scmi_gpu/clk_rate
```

---

## 8. Known-Good Baselines

### dress-up.riv — rk3566_player (generic)

| Resolution | FPS | Notes |
|------------|-----|-------|
| 384x384 | 60 | Vsync cap |
| 448x448 | ~51 | |
| 500x500 | **~47** | Standard benchmark |
| 600x600 | ~41 | |
| 700x700 | ~33 | |

### avatar.riv — avatar_tracker_demo (overlay mode)

| Resolution | FPS | Notes |
|------------|-----|-------|
| 360x360 | 60 | Vsync cap |
| 400x400 | 53 | |
| 500x500 | **~39** | Standard benchmark |

See `docs/FPS.md` for full benchmarks including async flip results.

Stack: Mesa 26.1.0-devel Panfrost, EXT-native PLS, GPU @800MHz.
