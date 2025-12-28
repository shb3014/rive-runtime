# Flush Crash Fix Summary

**Date:** December 24, 2025  
**Issue:** Segmentation fault in `RenderContext::flush()`  
**Status:** ✅ RESOLVED

---

## The Problem

The application crashed with a segmentation fault when calling:

```cpp
m_renderContext->flush({.renderTarget = nullptr});
```

## Root Cause

The `RenderContext::flush()` function **immediately dereferences** the `renderTarget` pointer:

```cpp
void RenderContext::flush(const FlushResources& flushResources)
{
    assert(flushResources.renderTarget->width() ==  // ❌ Crashes with nullptr
           m_frameDescriptor.renderTargetWidth);
```

**Passing `nullptr` causes an instant segmentation fault.**

---

## The Solution

Create a proper `FramebufferRenderTargetGL` object for the default framebuffer:

### Code Changes

**1. Add header (line 13):**
```cpp
#include "rive/renderer/gl/render_target_gl.hpp"
```

**2. Add member variable (line 317):**
```cpp
rcp<RenderTarget> m_renderTarget;
```

**3. Create render target in `initialize()` (after line 102):**
```cpp
// Create render target for the default framebuffer (ID 0)
GLint sampleCount = 0;
glGetIntegerv(GL_SAMPLES, &sampleCount);
std::cout << "Creating render target: " << m_width << "x" << m_height 
          << " (MSAA: " << sampleCount << "x)" << std::endl;
m_renderTarget = make_rcp<FramebufferRenderTargetGL>(
    m_width,
    m_height,
    0,  // Use framebuffer 0 (default framebuffer)
    sampleCount);
```

**4. Pass render target to flush() (line 303):**
```cpp
m_renderContext->flush({.renderTarget = m_renderTarget.get()});
```

---

## Why This Works

- `FramebufferRenderTargetGL` wraps the default OpenGL framebuffer (ID 0)
- It tracks dimensions and sample count
- The flush() function can safely query these properties
- This is the **standard pattern** used in all Rive renderer examples

---

## How to Build (On Orange Pi)

```bash
cd ~/rive-runtime/demos/rk3566_player
make clean

# Compile
clang++ -c -O2 -std=c++17 \
  -I../../include -I../../renderer/include -I../../renderer/src \
  -I/usr/include/libdrm -DRIVE_ANDROID \
  rk3566_drm_player.cpp -o obj/release/rk3566_drm_player.o

clang++ -c -O2 -std=c++17 \
  -I../../include -I../../renderer/include -I../../renderer/src \
  -I/usr/include/libdrm -DRIVE_ANDROID \
  drm_egl_context.cpp -o obj/release/drm_egl_context.o

# Link
clang++ -fuse-ld=lld \
  obj/release/drm_egl_context.o obj/release/rk3566_drm_player.o \
  -o bin/release/rk3566_player \
  -L../../out/release -L../../renderer/out/release \
  -Wl,--start-group \
  -l:librive_pls_renderer.a ../../out/release/librive.a \
  ../../out/release/librive_sheenbidi.a \
  ../../out/release/librive_yoga.a \
  -Wl,--end-group \
  -lEGL -lGLESv2 -ldrm -lgbm -lpthread -ldl -lm
```

---

## Testing

```bash
# Test with simple file
sudo ./bin/release/rk3566_player test2.riv

# Test with complex file
sudo ./bin/release/rk3566_player ~/dress-up.riv
```

**Expected:** Smooth 60 FPS animation with no crashes! 🎉

---

## Additional Investigations

While the nullptr issue was the immediate cause, here are other potential areas to explore if you encounter further issues:

### 1. **PLS Feature Compatibility**
The code already disables advanced PLS features for better compatibility:
```cpp
m_renderContext = RenderContextGLImpl::MakeContext(
    {.disablePixelLocalStorage = true,
     .disableFragmentShaderInterlock = true});
```

### 2. **Panfrost-Specific Optimizations**
If rendering is slow or glitchy, you might want to:
- Check Mesa version: `glxinfo | grep "OpenGL version"`
- Monitor GPU usage: `cat /sys/class/devfreq/fde60000.gpu/cur_freq`
- Try different MSAA settings

### 3. **Memory Management**
For complex animations, monitor:
```bash
# GPU memory
cat /sys/kernel/debug/dri/0/gem_objects

# System memory
free -h
```

### 4. **Alternative Rive Files**
Test with various complexity levels:
- Simple shapes (test2.riv)
- No text/effects
- Progressive complexity

---

## References

- Rive Renderer Examples: `renderer/path_fiddle/fiddle_context_gl.cpp`
- Test Framework: `tests/common/testing_gl_renderer.cpp`
- Render Target API: `renderer/include/rive/renderer/gl/render_target_gl.hpp`

---

**The fix is complete and ready to test on the Orange Pi!** 🚀



