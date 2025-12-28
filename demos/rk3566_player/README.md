# RK3566 Rive Player Demo

A demo application for playing Rive animations on RK3566 (Orange Pi) using DRM/KMS and OpenGL ES 3.2.

## Features

- Direct DRM/KMS framebuffer access (no X11/Wayland required)
- Hardware-accelerated OpenGL ES 3.2 rendering via Mali G52 GPU
- Support for Rive state machines and animations
- Cross-compilation from development machine
- VSync support for smooth animation

## Prerequisites

### On Development Machine

- Ubuntu/Debian Linux (or WSL2 on Windows)
- ARM64 cross-compilation toolchain:
  ```bash
  sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
  ```
- Build tools:
  ```bash
  sudo apt install build-essential git
  ```

### On RK3566 Target Device

- Orange Pi OS 1.0.8 Bullseye (or similar)
- Mali GPU drivers (libmali.so)
- DRM/KMS support (kernel 5.10.160 has this)
- Required packages:
  ```bash
  sudo apt install libdrm2 libgbm1
  ```

## Build Instructions

### 1. Setup Sysroot

First, collect the necessary libraries and headers from your RK3566:

```bash
cd rive-runtime/build
./setup_rk3566_sysroot.sh <rk3566-ip-address>
```

This will create a sysroot in `build/rk3566_sysroot` with the necessary files.

### 2. Build Rive Runtime

Cross-compile the Rive runtime and renderer:

```bash
cd rive-runtime
./build/cross_compile_rk3566.sh release
```

This will build all necessary libraries in `out/arm64_release`.

### 3. Build Demo Player

Build the demo player application:

```bash
cd demos/rk3566_player
./build.sh release
```

The binary will be created at `demos/rk3566_player/rk3566_player`.

## Deployment

### Transfer to RK3566

```bash
# Copy the player binary
scp rk3566_player root@<rk3566-ip>:/home/orangepi/

# Copy your .riv animation files
scp /path/to/your/animation.riv root@<rk3566-ip>:/home/orangepi/
```

### Run on RK3566

```bash
ssh root@<rk3566-ip>
cd /home/orangepi

# Run as root (for DRM access) or add user to video group
sudo ./rk3566_player animation.riv
```

### Setup Non-Root Execution

To run without `sudo`, add your user to the video group:

```bash
sudo usermod -a -G video,render $USER
# Log out and back in for changes to take effect
```

## Usage

```bash
./rk3566_player <path-to-riv-file>
```

**Example:**
```bash
./rk3566_player /home/orangepi/my_animation.riv
```

Press `Ctrl+C` to quit.

## Troubleshooting

### "Failed to open DRM device"

- Ensure you're running as root or in the `video` group
- Check that `/dev/dri/card0` exists: `ls -l /dev/dri/`
- Verify Mali drivers are loaded: `lsmod | grep mali`

### "Failed to create EGL context"

- Verify OpenGL ES support: `glxinfo | grep "OpenGL ES"`
- Check Mali library: `strings /usr/lib/aarch64-linux-gnu/libmali.so | grep EGL_VERSION`
- Ensure libmali.so is properly linked to libEGL.so and libGLESv3.so

### "Failed to import Rive file"

- Verify the .riv file is valid and not corrupted
- Ensure the file path is correct
- Check file permissions: `ls -l animation.riv`

### Build Errors

- Ensure sysroot is properly populated: `ls build/rk3566_sysroot/usr/lib/aarch64-linux-gnu/libmali.so`
- Verify Rive libraries are built: `ls out/arm64_release/`
- Check cross-compiler is installed: `aarch64-linux-gnu-gcc --version`

## Performance Notes

### Mali G52 GPU

The RK3566's Mali G52 GPU supports:
- OpenGL ES 3.2
- Vulkan 1.1
- Hardware-accelerated vector graphics

### Optimization Tips

1. **Resolution**: For best performance, use native display resolution (usually 1920x1080)
2. **VSync**: VSync is enabled by default for smooth animation
3. **Complexity**: Keep .riv files optimized for embedded devices
4. **Texture Memory**: Monitor memory usage for large/complex animations

### Expected Performance

- Simple animations: 60 FPS at 1080p
- Complex animations: 30-60 FPS depending on complexity
- State machines: 60 FPS for most use cases

## Architecture

```
┌─────────────────────────────────────┐
│     RK3566 Demo Player App          │
├─────────────────────────────────────┤
│  Rive Runtime & Renderer (OpenGL ES)│
├─────────────────────────────────────┤
│         EGL Context                 │
├─────────────────────────────────────┤
│    DRM/KMS + GBM                    │
├─────────────────────────────────────┤
│    Mali G52 GPU Driver              │
│      (libmali.so)                   │
├─────────────────────────────────────┤
│         Kernel DRM/KMS              │
└─────────────────────────────────────┘
```

## Alternative: Wayland Backend

If DRM/KMS proves problematic, a Wayland backend can be used instead. This requires:
- Wayland compositor running
- Modified initialization code to create Wayland window
- Slightly higher overhead but easier setup

See the main Rive documentation for Wayland examples.

## License

This demo follows the Rive Runtime license. See the main repository LICENSE file.

## Support

For issues specific to RK3566:
- Check kernel logs: `dmesg | grep -i drm`
- Check Mali driver: `dmesg | grep -i mali`
- Verify OpenGL support: `LIBGL_DEBUG=verbose glxinfo`

For Rive-specific issues:
- Visit [rive.app](https://rive.app)
- Check the main Rive runtime repository

