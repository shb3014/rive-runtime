# RK3566 Rive Player - Implementation Summary

**Date**: December 20, 2025  
**Status**: ✅ Working - Native build running on device  
**Device**: Orange Pi RK3566 (192.168.1.45, orangepi/orangepi)

---

## Overview

Successfully implemented a Rive animation player for RK3566 (ARM64) using DRM/KMS + EGL + OpenGL ES. The player runs natively on the device and can display `.riv` animation files directly to the framebuffer.

**Key Achievement**: Full native build pipeline working, player successfully initializing and running on the RK3566 device.

---

## Hardware & Software Environment

### Target Device (RK3566)
- **SoC**: Rockchip RK3566 (ARM Cortex-A55 quad-core)
- **GPU**: Mali-G52 (hardware not currently active - see Known Issues)
- **OS**: Orange Pi OS 1.0.8 Bullseye (Linux 5.10.160)
- **Display**: 1280x720@60Hz
- **IP**: 192.168.1.45
- **Credentials**: orangepi / orangepi

### Development Machine
- **Platform**: WSL2 (Linux 5.15.90.1-microsoft-standard-WSL2)
- **Path**: `/home/shb3014/embeddedProjects/rive-runtime`

---

## Build Approach Evolution

### Initial Attempt: Cross-Compilation (Failed)
Tried cross-compiling from WSL2 using `aarch64-linux-gnu-gcc`, but encountered:
- GLIBC version mismatches (WSL2 GLIBC 2.35 vs device GLIBC 2.31)
- Incompatible libc_nonshared.a linkage
- Unsupported compiler flags (`-Wimplicit-int-conversion`, `-Wshorten-64-to-32`)

### Final Solution: Native Build (Success)
**Pivoted to building everything directly on the RK3566 device:**
- Cloned `rive-runtime` repository on device
- Built all Rive libraries natively using device's `gcc 10.2.1`
- Built demo player natively
- Avoided all cross-compilation toolchain mismatches

---

## Project Structure

```
rive-runtime/
├── demos/rk3566_player/          # Demo player (our code)
│   ├── rk3566_drm_player.cpp     # Main application
│   ├── drm_egl_context.cpp       # DRM/KMS + EGL context
│   ├── drm_egl_context.h
│   ├── premake5.lua              # Build configuration
│   └── build.sh                  # Build script
├── renderer/                      # Rive PLS renderer
│   ├── out/release/              # Built renderer libraries
│   └── glad/                     # OpenGL loader
├── out/release/                  # Built runtime libraries
└── dress-up.riv                  # Test animation file
```

---

## Key Technical Solutions

### 1. EGL Surface Creation (EGL_BAD_MATCH Fix)

**Problem**: `eglCreateWindowSurface()` failed with `EGL_BAD_MATCH (0x3009)`

**Root Cause**: Mismatch between GBM surface pixel format and EGL config

**Solution** (`demos/rk3566_player/drm_egl_context.cpp`):
```cpp
// Changed GBM format from XRGB8888 to ARGB8888
m_gbmSurface = gbm_surface_create(m_gbmDevice,
                                   mode->hdisplay,
                                   mode->vdisplay,
                                   GBM_FORMAT_ARGB8888,  // Was XRGB8888
                                   GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);

// Updated EGL config to request alpha channel
EGLint configAttribs[] = {
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
    EGL_RED_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_ALPHA_SIZE, 8,          // Was 0 - must match ARGB8888
    EGL_DEPTH_SIZE, 0,
    EGL_STENCIL_SIZE, 8,
    EGL_NATIVE_VISUAL_ID, GBM_FORMAT_ARGB8888,  // Added for format matching
    EGL_NONE
};
```

### 2. GLAD Loader Initialization (Segfault Fix)

**Problem**: Segmentation fault (PC=0x0) in `RenderContextGLImpl::MakeContext()`

**Root Cause**: On Linux, Rive's renderer is built with `RIVE_DESKTOP_GL`, which uses GLAD for OpenGL function loading. Without calling `gladLoadCustomLoader()`, all GL function pointers are NULL.

**Solution** (`demos/rk3566_player/rk3566_drm_player.cpp`):
```cpp
// Forward-declare GLAD loader (avoid header conflicts with system GLES)
extern "C" {
    typedef void* (*GLADloadfunc)(const char* name);
    typedef void (*GLADapiproc)(void);
    int gladLoadCustomLoader(GLADloadfunc);
}

static void* (*g_eglGetProcAddress)(const char*) = nullptr;
static GLADapiproc gladEglLoader(const char* name)
{
    return (GLADapiproc)(g_eglGetProcAddress ? g_eglGetProcAddress(name)
                                             : nullptr);
}

// In RK3566Player::initialize():
g_eglGetProcAddress = m_drmContext->getProcAddress();
if (g_eglGetProcAddress == nullptr ||
    gladLoadCustomLoader(gladEglLoader) == 0)
{
    std::cerr << "Failed to load OpenGL ES functions via GLAD" << std::endl;
    return false;
}
```

### 3. RTTI/Typeinfo Linker Errors

**Problem**: Undefined references to `typeinfo for rive::StateMachineInstance`, etc.

**Root Cause**: `dynamic_cast` requires RTTI, which can cause linker issues when libraries are built with different RTTI settings.

**Solution**: Removed `dynamic_cast` usage, replaced with simpler checks:
```cpp
// OLD (caused linker errors):
if (auto* sm = dynamic_cast<StateMachineInstance*>(m_scene.get())) { ... }

// NEW (works):
if (m_scene)
{
    std::cout << "Scene: " << m_scene->name() << std::endl;
}
```

### 4. Mesa/llvmpipe Compatibility

**Problem**: Device is using Mesa llvmpipe (software renderer) instead of Mali GPU

**Solution**: Disabled advanced rendering features that llvmpipe doesn't support well:
```cpp
m_renderContext = RenderContextGLImpl::MakeContext(
    {.disablePixelLocalStorage = true,
     .disableFragmentShaderInterlock = true});
```

### 5. DRM Connector/CRTC Finding

**Problem**: `drmModeConnector` on device doesn't have `possible_encoders` field

**Solution** (`demos/rk3566_player/drm_egl_context.cpp`):
```cpp
// Simplified CRTC finding - iterate through all CRTCs directly
uint32_t DRMEGLContext::findCRTC(drmModeRes* resources,
                                  drmModeConnector* connector)
{
    for (int i = 0; i < resources->count_crtcs; i++)
    {
        if (!(m_crtcAllocMask & (1 << i)))
        {
            m_crtcAllocMask |= (1 << i);
            return resources->crtcs[i];
        }
    }
    return 0;
}
```

---

## Build Process (On RK3566)

### Prerequisites Installed on Device
```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    git \
    libdrm-dev \
    libgbm-dev \
    libegl1-mesa-dev \
    libgles2-mesa-dev \
    uuid-dev \
    libjpeg-dev \
    libpng-dev \
    libwebp-dev \
    zlib1g-dev
```

### Build Commands (On RK3566)
```bash
# Navigate to project
cd ~/rive-runtime

# Build Rive runtime library
cd ~/rive-runtime
./build/build_rive.sh

# Build Rive renderer library
cd ~/rive-runtime/renderer
./build.sh

# Build demo player
cd ~/rive-runtime/demos/rk3566_player
./build.sh
```

### Build Modifications Made

**`demos/rk3566_player/premake5.lua`**:
- Added `rive_decoders` library for image decoding
- Added `--start-group`/`--end-group` linker flags for circular dependencies
- Linked against: `rive`, `rive_harfbuzz`, `rive_sheenbidi`, `rive_pls_renderer`, `rive_decoders`
- System libraries: `drm`, `gbm`, `EGL`, `GLESv2`, `jpeg`, `webp`, `png`, `z`

**`demos/rk3566_player/rk3566_drm_player.cpp`**:
- Added GLAD loader initialization before creating Rive renderer
- Removed `dynamic_cast` usage to avoid RTTI linker errors
- Set renderer options for llvmpipe compatibility

**`demos/rk3566_player/drm_egl_context.cpp`**:
- Changed pixel format to `GBM_FORMAT_ARGB8888`
- Updated EGL config to request alpha channel
- Simplified CRTC finding logic
- Request OpenGL ES 3.0 context (compatible with ES 3.2)

---

## Running the Player

### Binary Location
```
/home/orangepi/rive-runtime/demos/rk3566_player/bin/release/rk3566_player
```

### Run Command
```bash
cd ~/rive-runtime
sudo ./demos/rk3566_player/bin/release/rk3566_player ./dress-up.riv
```

**Note**: Requires `sudo` for DRM/KMS access.

### Expected Output (Working)
```
=== RK3566 Rive Player ===

Initializing DRM/EGL...
Opened DRM device: /dev/dri/card0
Found display: 1280x720@60Hz
EGL 1.4 initialized
Created OpenGL ES 3.0 context
DRM/EGL initialized successfully:
  Display: 1280x720@60Hz
  EGL Version: 1.4
  EGL Vendor: Mesa Project
  GL Version: OpenGL ES 3.2 Mesa 20.3.5
  GL Renderer: llvmpipe (LLVM 11.0.1, 128 bits)

Initializing Rive renderer...
Rive renderer initialized

Loading Rive file: ./dress-up.riv
Read 513391 bytes from file

=== Initialization Complete ===
Display: 1280x720
Artboard: scene
Size: 800x800
Scene: State Machine 1

=== Starting Render Loop ===
Press Ctrl+C to quit
```

---

## Known Issues & Limitations

### 1. Using Software Rendering (llvmpipe)

**Current Status**:
- GL Renderer reports: `llvmpipe (LLVM 11.0.1, 128 bits)`
- Not using Mali-G52 hardware GPU

**Impact**:
- ✅ Everything works, no crashes
- ❌ Poor performance (software rendering)
- ❌ High CPU usage

**Root Cause**:
- Mali kernel driver not loaded: no `/dev/mali0`
- Mali userspace libraries missing: no `/usr/lib/libmali.so`
- EGL/GLESv2 are Mesa implementations (software fallback)

**Next Steps to Enable Hardware GPU**:
1. **Check if Mali kernel module exists**:
   ```bash
   ls /lib/modules/$(uname -r)/kernel/drivers/gpu/arm/
   modinfo mali  # or panfrost
   ```

2. **Load Mali kernel module** (if available):
   ```bash
   sudo modprobe mali  # or panfrost
   # Check for /dev/mali0
   ```

3. **Install Mali userspace libraries**:
   - Orange Pi may provide a package: `libmali-*-gbm.so`
   - Or compile from Rockchip BSP sources
   - Should provide: `/usr/lib/aarch64-linux-gnu/libmali.so`

4. **Configure Mesa to use Mali**:
   ```bash
   # Set to prefer hardware driver
   export MESA_LOADER_DRIVER_OVERRIDE=mali
   # Or remove Mesa GL libraries if Mali provides full stack
   ```

### 2. Performance Not Measured

**Status**: Player runs but no FPS measurement implemented

**To Add**: Modify render loop to print frame time/FPS statistics

### 3. No Display Output Verification

**Status**: Render loop executes, but not confirmed if pixels reach the screen

**Reason**: Testing via SSH, no physical display verification

**To Verify**: Check physical HDMI output on the device

---

## File Modifications Summary

### Created Files
- `demos/rk3566_player/rk3566_drm_player.cpp` - Main player application
- `demos/rk3566_player/drm_egl_context.cpp` - DRM/EGL context implementation
- `demos/rk3566_player/drm_egl_context.h` - DRM/EGL context header
- `demos/rk3566_player/premake5.lua` - Build configuration
- `demos/rk3566_player/build.sh` - Build script

### Key Code Changes

**`rk3566_drm_player.cpp`** (Main changes):
- Added GLAD loader initialization (lines 23-31, 81-87)
- Changed `RenderContextGLImpl::MakeContext()` options (line 89-90)
- Removed `dynamic_cast` for scene type detection (line 118-121)

**`drm_egl_context.cpp`** (Main changes):
- Changed `GBM_FORMAT_XRGB8888` → `GBM_FORMAT_ARGB8888` (line 229)
- Updated EGL config: `EGL_ALPHA_SIZE, 8` (line 141)
- Added `EGL_NATIVE_VISUAL_ID` to EGL config (line 143)
- Request ES 3.0 context with 3.2 fallback (lines 148-153)
- Simplified `findCRTC()` logic (lines 208-218)

**`premake5.lua`** (Main changes):
- Added library dependencies (lines 35-47)
- Added `--start-group`/`--end-group` linker flags (lines 49-50)
- Set library search paths (lines 30-33)

---

## Build System Details

### Premake5 Configuration
- **Generator**: Premake5 (built from source during first run)
- **Build Tool**: GNU Make
- **Compiler**: gcc 10.2.1 (native on device)
- **Target**: release configuration

### Library Dependencies (Linking Order)
```
rive_pls_renderer → rive → rive_harfbuzz → rive_sheenbidi
                  → rive_decoders (for Bitmap support)
System: drm, gbm, EGL, GLESv2, jpeg, webp, png, z
```

### Compilation Flags
- `-std=c++17`
- `-O3` (release)
- `-g` (debug symbols available)
- RTTI enabled (required by Rive runtime)

---

## Architecture Overview

### Rendering Pipeline
```
Application (rk3566_drm_player)
    ↓
DRMEGLContext (DRM/KMS + EGL)
    ↓
GBM Surface (shared buffer)
    ↓
OpenGL ES 3.0 Context (currently llvmpipe)
    ↓
Rive PLS Renderer (rive_pls_renderer)
    ↓
Rive Runtime (animation playback)
    ↓
Framebuffer (direct scanout)
```

### Key Components

1. **DRMEGLContext**: Manages low-level display/graphics setup
   - Opens `/dev/dri/card0` (DRM device)
   - Finds connected display and mode (1280x720@60Hz)
   - Creates GBM device and surface
   - Initializes EGL and OpenGL ES context

2. **RK3566Player**: Application logic
   - Loads `.riv` file via Rive's `File::import()`
   - Creates `ArtboardInstance` and default `Scene`
   - Advances animation time each frame
   - Renders via Rive's renderer

3. **Rive Renderer**: Hardware-accelerated path rendering
   - Uses OpenGL ES 3.0+ features
   - MSAA-based rendering path (since PLS/interlock disabled)
   - Handles gradients, images, text via harfbuzz

---

## Testing Results

### ✅ Working
- DRM device opening and mode detection
- EGL/GBM surface creation
- OpenGL ES 3.0 context creation
- GLAD loader initialization
- Rive renderer initialization
- .riv file loading and parsing
- Scene/artboard creation
- Render loop execution (no crashes after 12+ seconds)

### ⚠️ Unknown/Untested
- Actual display output (no physical screen verification)
- Animation correctness (no visual inspection)
- Frame rate / performance metrics
- Multi-artboard support
- Touch input / state machine interaction

### ❌ Known Limitations
- Using software rendering (llvmpipe) instead of Mali GPU
- No FPS counter or performance measurement
- No error recovery if .riv file is missing/corrupt

---

## Next Steps

### Immediate
1. **Verify physical display output** - Connect HDMI and confirm animation is visible
2. **Add FPS counter** - Measure actual rendering performance
3. **Enable Mali GPU driver** - Switch from llvmpipe to hardware acceleration

### GPU Activation Checklist
```bash
# 1. Check if kernel module exists
find /lib/modules -name "*mali*" -o -name "*panfrost*"

# 2. Check if Mali libraries are installed
find /usr/lib -name "*mali*"

# 3. Try loading kernel module
sudo modprobe mali
# or
sudo modprobe panfrost

# 4. Verify GPU device
ls -l /dev/mali* /dev/dri/*

# 5. Check Mesa configuration
glxinfo | grep -i opengl  # if X is available
# or
EGL_LOG_LEVEL=debug ./rk3566_player test.riv
```

### Future Enhancements
- Add command-line options (--width, --height, --fps-limit)
- Support multiple .riv files / playlist mode
- Add state machine input handling (touch/mouse)
- Implement vsync-based frame pacing
- Add graceful shutdown on SIGTERM
- Create systemd service for auto-start

---

## References

### Documentation
- [Rive Runtime API](https://github.com/rive-app/rive-runtime)
- [DRM/KMS Programming](https://www.kernel.org/doc/html/latest/gpu/drm-kms.html)
- [EGL 1.4 Specification](https://www.khronos.org/registry/EGL/specs/eglspec.1.4.pdf)
- [OpenGL ES 3.0 Specification](https://www.khronos.org/registry/OpenGL/specs/es/3.0/es_spec_3.0.pdf)

### Related Files
- Original plan: `PROJECT_CONTEXT_RK3566_RIVE.md`
- This summary: `IMPLEMENTATION_SUMMARY.md`

---

## Contact & Credentials

**RK3566 Device Access**:
- IP: 192.168.1.45
- Username: orangepi
- Password: orangepi
- SSH: `ssh orangepi@192.168.1.45`
- SCP: `scp file orangepi@192.168.1.45:~/path/`

**Project Location**:
- WSL2: `/home/shb3014/embeddedProjects/rive-runtime`
- Device: `/home/orangepi/rive-runtime`

---

*Last Updated: December 20, 2025*







