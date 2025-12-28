# Native Build Guide for Orange Pi 3B

## Current Status

✅ **Completed**:
- SSH access configured (192.168.1.45, user: ubuntu, pass: shb084ww)
- DRM devices verified (`/dev/dri/card0`, `/dev/dri/renderD128`)
- Build tools installation started on Orange Pi

⏳ **In Progress**:
- Installing `build-essential`, `git`, `cmake` on Orange Pi

## Why Native Build?

Cross-compilation failed due to:
1. Rive PLS renderer was built with text support (HarfBuzz)
2. HarfBuzz compilation requires GCC 12+ (`-Wimplicit-int-conversion` flag)
3. Ubuntu 22.04 WSL has GCC 11, causing ~200 undefined reference errors

Native build avoids all cross-compilation issues.

##Step-by-Step Native Build Process

### 1. Complete Build Tools Installation

SSH to Orange Pi and complete installation:

```bash
ssh ubuntu@192.168.1.45
# Password: shb084ww

# Wait for any running apt processes to finish
ps aux | grep apt

# Install required packages
sudo apt update
sudo apt install -y build-essential git cmake pkg-config \
    libdrm-dev libgbm-dev

# Verify installation
gcc --version
cmake --version
```

### 2. Clone Rive Runtime on Orange Pi

```bash
cd ~
git clone https://github.com/rive-app/rive-runtime.git
cd rive-runtime
```

### 3. Build Rive Runtime (Without Text Support)

To avoid HarfBuzz complexity, build without text/layout support:

```bash
cd ~/rive-runtime
./build/build_rive.sh release
```

This will build:
- `out/release/librive.a`
- `out/release/librive_pls_renderer.a`
- `out/release/librive_sheenbidi.a`
- `out/release/librive_yoga.a`

**Expected build time**: 15-30 minutes

### 4. Copy Demo Player Source

From your WSL machine, copy the demo player files:

```bash
cd /home/shb3014/embeddedProjects/rive-runtime
scp -r demos/rk3566_player ubuntu@192.168.1.45:~/rive-runtime/demos/
```

### 5. Build Demo Player on Orange Pi

```bash
ssh ubuntu@192.168.1.45
cd ~/rive-runtime/demos/rk3566_player
./build.sh release
```

### 6. Copy Test File and Run

```bash
# From WSL:
scp /home/shb3014/embeddedProjects/rive-runtime/dress-up.riv ubuntu@192.168.1.45:~/

# On Orange Pi:
ssh ubuntu@192.168.1.45
cd ~
sudo ./rive-runtime/demos/rk3566_player/rk3566_player dress-up.riv
```

## Troubleshooting

### Issue: "Failed to open DRM device"

**Solution**: Add user to video group:
```bash
sudo usermod -a -G video,render ubuntu
# Logout and login again
```

### Issue: "Failed to initialize EGL"

**Check GL libraries**:
```bash
ldconfig -p | grep -E 'EGL|GLES'
ls -la /usr/lib/aarch64-linux-gnu/libEGL* /usr/lib/aarch64-linux-gnu/libGLES*
```

**If missing**, install:
```bash
sudo apt install -y libgl1 libgles2 libegl1
```

###Issue: Build fails with "cannot find -lEGL"

The minimal Ubuntu server doesn't have GL libraries. Two options:

**Option A**: Install desktop with GPU drivers:
```bash
sudo apt install -y ubuntu-desktop
# Reboot
```

**Option B**: Use software rendering:
The demo will still compile and run, but may use CPU rendering instead of GPU.

### Issue: Low FPS

Check GPU status:
```bash
# Check if GPU is detected
ls /dev/dri/
lsmod | grep -i gpu

# Check GPU frequency
cat /sys/class/devfreq/*/cur_freq

# Set performance mode
echo performance | sudo tee /sys/class/devfreq/*/governor
```

## Alternative: Simplified Deployment

If you encounter issues with the full Rive build, I can create a simplified version that:
- Uses only software rendering
- Removes text rendering dependencies
- Focuses on basic vector animation playback

Let me know if you'd like me to create this simplified version!

## Expected Results

When successful, you should see:

```
=== RK3566 Rive Player ===

Initializing DRM/EGL...
Opened DRM device: /dev/dri/card0
Found display: 1920x1080@60Hz
DRM/EGL initialized successfully:
  Display: 1920x1080@60Hz
  EGL Version: 1.4
  EGL Vendor: Mesa Project (or similar)
  GL Version: OpenGL ES 3.x
  GL Renderer: (GPU name)

Loading Rive file: dress-up.riv
Read 513391 bytes from file

=== Initialization Complete ===
Display: 1920x1080
Artboard: (name)
Size: (dimensions)

=== Starting Render Loop ===
Press Ctrl+C to quit
FPS: 60.0 (16.7 ms/frame)
```

## Contact

If you encounter issues, share:
1. Error messages from build
2. Output of `ldd rk3566_player` (if binary was built)
3. Output of `glxinfo | head -20` (if available)
4. Output of `ls -la /dev/dri/`





