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

## 4. Run the Player

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

## 5. Render Resolution Knobs

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

## 6. Diagnostics

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

## 7. Known-Good Baseline (dress-up.riv)

| Resolution | FPS | Notes |
|------------|-----|-------|
| 384x384 | 60 | Vsync cap |
| 448x448 | ~51 | |
| 500x500 | **~47** | Standard benchmark |
| 600x600 | ~41 | |
| 700x700 | ~33 | |

Stack: Mesa 26.1.0-devel Panfrost, EXT-native PLS, GPU @800MHz.
