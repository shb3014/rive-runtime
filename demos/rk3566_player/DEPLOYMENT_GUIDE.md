# RK3566 Rive Deployment Guide

Complete step-by-step guide to deploy Rive runtime on RK3566 (Orange Pi) with OpenGL ES 3.2 support.

## Table of Contents

1. [Environment Setup](#1-environment-setup)
2. [Sysroot Collection](#2-sysroot-collection)
3. [Build Rive Libraries](#3-build-rive-libraries)
4. [Build Demo Player](#4-build-demo-player)
5. [Deployment](#5-deployment)
6. [Testing](#6-testing)
7. [Troubleshooting](#7-troubleshooting)

---

## 1. Environment Setup

### On Your Development Machine (Windows with WSL2 or Linux)

#### Install WSL2 (Windows users only)

```powershell
# In PowerShell as Administrator
wsl --install -d Ubuntu-22.04
```

Restart your computer after installation.

#### Install Cross-Compilation Toolchain

```bash
# In WSL2/Linux terminal
sudo apt update
sudo apt install -y \
    gcc-aarch64-linux-gnu \
    g++-aarch64-linux-gnu \
    build-essential \
    git \
    cmake \
    pkg-config \
    libdrm-dev \
    libgbm-dev

# Verify installation
aarch64-linux-gnu-gcc --version
```

Expected output:
```
aarch64-linux-gnu-gcc (Ubuntu ...) ...
```

#### Clone Rive Runtime Repository

```bash
cd ~
git clone https://github.com/rive-app/rive-runtime.git
cd rive-runtime
```

---

## 2. Sysroot Collection

The sysroot contains libraries and headers from your RK3566 device needed for cross-compilation.

### Prepare RK3566 Device

On your RK3566, ensure SSH is enabled and you know the IP address:

```bash
# On RK3566
ip addr show
# Note the IP address (e.g., 192.168.1.100)

# Install required packages
sudo apt update
sudo apt install -y libdrm2 libgbm1 libegl1 libgles2
```

### Collect Sysroot Files

On your development machine:

```bash
cd ~/rive-runtime/build

# Make script executable
chmod +x setup_rk3566_sysroot.sh

# Run sysroot setup (replace with your RK3566 IP)
./setup_rk3566_sysroot.sh 192.168.1.100
```

You'll be prompted for the root password several times during file copying.

**Expected output:**
```
=== Setting up RK3566 Sysroot ===
Target device: 192.168.1.100
...
✓ usr/lib/aarch64-linux-gnu/libmali.so
✓ usr/include/EGL/egl.h
✓ usr/include/GLES3/gl3.h
✓ All critical files present!
```

### Verify Sysroot

```bash
ls -l build/rk3566_sysroot/usr/lib/aarch64-linux-gnu/libmali.so*
ls -l build/rk3566_sysroot/usr/include/EGL/
```

---

## 3. Build Rive Libraries

### Set Environment Variables

```bash
cd ~/rive-runtime

export SYSROOT="$(pwd)/build/rk3566_sysroot"
export CC=aarch64-linux-gnu-gcc
export CXX=aarch64-linux-gnu-g++
```

### Run Cross-Compilation Build

```bash
# Make script executable
chmod +x build/cross_compile_rk3566.sh

# Build release version
./build/cross_compile_rk3566.sh release
```

This will:
1. Download and build premake5
2. Configure the build system
3. Build Rive runtime
4. Build Rive renderer with OpenGL ES support
5. Build all dependencies (libpng, harfbuzz, yoga, etc.)

**Build time:** 10-30 minutes depending on your system.

**Expected output:**
```
=== RK3566 Cross-Compilation Setup ===
✓ Cross-compiler found: aarch64-linux-gnu-gcc ...
✓ Found: libmali.so
✓ Found: libEGL.so
...
=== Build Complete ===
Libraries are in: /home/.../rive-runtime/out/arm64_release
```

### Verify Build

```bash
ls -lh out/arm64_release/
```

You should see files like:
- `librive.a`
- `librive_pls_renderer.a`
- `librive_harfbuzz.a`
- etc.

---

## 4. Build Demo Player

### Navigate to Demo Directory

```bash
cd ~/rive-runtime/demos/rk3566_player
```

### Make Build Script Executable

```bash
chmod +x build.sh
```

### Build the Player

```bash
# Ensure environment is set
export SYSROOT="$(pwd)/../../build/rk3566_sysroot"

# Build
./build.sh release
```

**Expected output:**
```
=== Building RK3566 Demo Player ===
Using Rive libraries from: .../out/arm64_release
...
=== Build Complete ===
Binary: .../demos/rk3566_player/rk3566_player
```

### Verify Binary

```bash
ls -lh rk3566_player
file rk3566_player
```

Expected output:
```
rk3566_player: ELF 64-bit LSB executable, ARM aarch64, ...
```

---

## 5. Deployment

### Prepare RK3566 Device

On your RK3566:

```bash
# Create directory for demos
mkdir -p ~/rive_demos
cd ~/rive_demos

# Ensure you're in video group for DRM access
sudo usermod -a -G video,render $USER
sudo usermod -a -G video,render orangepi

# Verify Mali driver is loaded
lsmod | grep mali
# Should show: mali_kbase or similar

# Check DRM devices
ls -l /dev/dri/
# Should show card0 and renderD128
```

Log out and back in for group changes to take effect.

### Transfer Files

From your development machine:

```bash
cd ~/rive-runtime/demos/rk3566_player

# Transfer the player binary
scp rk3566_player orangepi@192.168.1.100:~/rive_demos/

# Transfer a test .riv file (download from rive.app if needed)
# Example: download a .riv from https://rive.app/community
scp ~/Downloads/example.riv orangepi@192.168.1.100:~/rive_demos/
```

### Set Permissions on RK3566

```bash
# On RK3566
chmod +x ~/rive_demos/rk3566_player
```

---

## 6. Testing

### Basic Test

On your RK3566:

```bash
cd ~/rive_demos

# First test with sudo (for DRM master access)
sudo ./rk3566_player example.riv
```

**Expected output:**
```
=== RK3566 Rive Player ===

Initializing DRM/EGL...
Opened DRM device: /dev/dri/card0
Found display: 1920x1080@60Hz
EGL 1.4 initialized
...
=== Initialization Complete ===
Display: 1920x1080
Artboard: YourArtboardName
...
=== Starting Render Loop ===
Press Ctrl+C to quit
FPS: 59.8 (16.7 ms/frame)
FPS: 60.1 (16.6 ms/frame)
```

Press `Ctrl+C` to exit.

### Test Without Sudo (After Group Setup)

```bash
# Log out and back in first, then:
./rk3566_player example.riv
```

### Performance Test

Monitor system resources:

```bash
# In another SSH session
top -p $(pgrep rk3566_player)

# Check GPU usage (if available)
cat /sys/class/devfreq/fde60000.gpu/cur_freq
```

---

## 7. Troubleshooting

### Issue: "Failed to open DRM device"

**Symptoms:**
```
Failed to open DRM device. Are you running as root or in video group?
```

**Solutions:**
1. Run with sudo: `sudo ./rk3566_player file.riv`
2. Add user to groups:
   ```bash
   sudo usermod -a -G video,render $USER
   # Log out and back in
   ```
3. Check device exists: `ls -l /dev/dri/card0`

### Issue: "Failed to initialize EGL"

**Symptoms:**
```
Failed to initialize EGL
```

**Solutions:**
1. Verify Mali library:
   ```bash
   ls -l /usr/lib/aarch64-linux-gnu/libmali.so*
   ldconfig -p | grep mali
   ```
2. Check EGL support:
   ```bash
   strings /usr/lib/aarch64-linux-gnu/libmali.so | grep EGL_VERSION
   ```
3. Install missing packages:
   ```bash
   sudo apt install libegl1 libgles2 libgbm1 libdrm2
   ```

### Issue: "Failed to import Rive file"

**Symptoms:**
```
Failed to import Rive file
```

**Solutions:**
1. Verify file exists and is readable:
   ```bash
   ls -l example.riv
   file example.riv
   ```
2. Try a different .riv file from rive.app
3. Check file isn't corrupted:
   ```bash
   md5sum example.riv
   ```

### Issue: Low FPS / Poor Performance

**Solutions:**
1. Check GPU frequency:
   ```bash
   cat /sys/class/devfreq/fde60000.gpu/cur_freq
   cat /sys/class/devfreq/fde60000.gpu/available_frequencies
   ```
2. Set performance governor:
   ```bash
   echo performance | sudo tee /sys/class/devfreq/fde60000.gpu/governor
   ```
3. Monitor CPU/GPU temperature:
   ```bash
   cat /sys/class/thermal/thermal_zone0/temp
   ```
4. Simplify the .riv animation
5. Lower display resolution

### Issue: Display Shows Nothing

**Solutions:**
1. Check if another process is using DRM:
   ```bash
   fuser /dev/dri/card0
   ```
2. Stop X11/Wayland if running:
   ```bash
   sudo systemctl stop lightdm  # or gdm, or sddm
   ```
3. Switch to TTY: Press `Ctrl+Alt+F2`
4. Check kernel logs:
   ```bash
   dmesg | tail -50
   ```

### Issue: Build Failures

**For Rive runtime build:**
1. Verify sysroot: `ls build/rk3566_sysroot/usr/lib/aarch64-linux-gnu/`
2. Check toolchain: `aarch64-linux-gnu-gcc --version`
3. Clean and rebuild:
   ```bash
   rm -rf out/arm64_release
   ./build/cross_compile_rk3566.sh release
   ```

**For demo player build:**
1. Ensure Rive libs exist: `ls out/arm64_release/librive.a`
2. Clean and rebuild:
   ```bash
   cd demos/rk3566_player
   rm -rf bin obj Makefile
   ./build.sh release
   ```

---

## Advanced Topics

### Custom Resolution

Modify the DRM/EGL context initialization to use a specific mode:

```cpp
// In drm_egl_context.cpp, findDisplay()
// Select a different mode from connector->modes[]
```

### Multiple Displays

Currently supports primary display only. For multiple displays:
1. Enumerate all connectors in `findDisplay()`
2. Create separate contexts per display
3. Manage multiple render loops

### Wayland Alternative

If DRM/KMS is problematic, use Wayland:
1. Install Wayland: `sudo apt install wayland-protocols libwayland-dev`
2. Modify code to use Wayland EGL instead of GBM
3. Requires compositor (weston, sway, etc.)

### Performance Profiling

Use system tools to profile:

```bash
# CPU profiling
perf record -g ./rk3566_player file.riv
perf report

# GPU profiling (if Mali tools available)
# Requires ARM Streamline or similar
```

---

## Sample .riv Files

Download free animations from:
- https://rive.app/community/
- https://rive.app/examples/

Or create your own at:
- https://rive.app/

---

## Next Steps

After successful deployment:

1. **Integrate into Your Application:** Use the demo as reference
2. **Add Input Handling:** Touchscreen, mouse, keyboard support
3. **Add Multiple Artboards:** Switch between different animations
4. **State Machine Interaction:** Add triggers and state changes
5. **Optimize Performance:** Profile and tune for your use case

---

## Support

- **Rive Documentation:** https://rive.app/docs
- **Rive Community:** https://rive.app/community/
- **GitHub Issues:** https://github.com/rive-app/rive-runtime/issues

For RK3566-specific issues, check:
- Orange Pi forums
- Rockchip documentation
- Mali GPU documentation

