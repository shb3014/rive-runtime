/*
 * Copyright 2025 Rive
 * RK3566 DRM/KMS Rive Player Demo
 */

#include "drm_egl_context.h"
#include "rive/artboard.hpp"
#include "rive/file.hpp"
#include "rive/animation/linear_animation_instance.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include "rive/renderer/gl/render_context_gl_impl.hpp"
#include "rive/renderer/gl/render_target_gl.hpp"
#include "rive/math/aabb.hpp"
#include <GLES3/gl3.h>

// Our `librive_pls_renderer.a` is built on Linux with `RIVE_DESKTOP_GL`, which
// requires consumers to call `gladLoadCustomLoader()` before using GL entry
// points. We avoid including GLAD headers here (they conflict with GLES3
// headers) and just forward declare the minimal API.
extern "C"
{
    using GLADapiproc = void (*)();
    using GLADloadfunc = GLADapiproc (*)(const char* name);
    int gladLoadCustomLoader(GLADloadfunc);
}

static void* (*g_eglGetProcAddress)(const char*) = nullptr;
static GLADapiproc gladEglLoader(const char* name)
{
    return (GLADapiproc)(g_eglGetProcAddress ? g_eglGetProcAddress(name)
                                             : nullptr);
}

#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include <memory>
#include <iomanip>
#include <algorithm>
#include <signal.h>
#include <cstring>
#include <strings.h>
#include <cstdlib>

using namespace rive;
using namespace rive::gpu;

// Global flag for graceful shutdown
static volatile bool g_running = true;

static bool envFlag(const char* name, bool defaultValue = false)
{
    const char* v = std::getenv(name);
    if (v == nullptr)
    {
        return defaultValue;
    }
    // Treat empty as enabled (common pattern in shells), and accept 1/true/yes.
    if (*v == '\0')
    {
        return true;
    }
    if (!strcasecmp(v, "1") || !strcasecmp(v, "true") || !strcasecmp(v, "yes") ||
        !strcasecmp(v, "on"))
    {
        return true;
    }
    if (!strcasecmp(v, "0") || !strcasecmp(v, "false") || !strcasecmp(v, "no") ||
        !strcasecmp(v, "off"))
    {
        return false;
    }
    // Fallback: any other value -> default
    return defaultValue;
}

static uint32_t envU32(const char* name, uint32_t defaultValue = 0)
{
    const char* v = std::getenv(name);
    if (v == nullptr || *v == '\0')
    {
        return defaultValue;
    }
    char* end = nullptr;
    unsigned long parsed = std::strtoul(v, &end, 10);
    if (end == v)
    {
        return defaultValue;
    }
    return static_cast<uint32_t>(parsed);
}

static float envFloat(const char* name, float defaultValue)
{
    const char* v = std::getenv(name);
    if (v == nullptr || *v == '\0')
    {
        return defaultValue;
    }
    char* end = nullptr;
    float parsed = std::strtof(v, &end);
    if (end == v)
    {
        return defaultValue;
    }
    return parsed;
}

void signalHandler(int signum)
{
    std::cout << "\nInterrupt signal (" << signum << ") received.\n";
    g_running = false;
}

class RK3566Player
{
public:
    RK3566Player() = default;
    ~RK3566Player() = default;

    bool initialize(const char* rivPath)
    {
        std::cout << "=== RK3566 Rive Player ===" << std::endl;
        
        // Initialize FPS counter
        m_lastFpsReport = std::chrono::steady_clock::now();

        // Initialize DRM/EGL context
        std::cout << "\nInitializing DRM/EGL..." << std::endl;
        m_drmContext = std::make_unique<rive_rk3566::DRMEGLContext>();
        if (!m_drmContext->initialize())
        {
            std::cerr << "Failed to initialize DRM/EGL: "
                      << m_drmContext->lastError() << std::endl;
            return false;
        }

        m_width = m_drmContext->width();
        m_height = m_drmContext->height();
        m_riveWidth = m_width;
        m_riveHeight = m_height;

        // Create Rive renderer context
        std::cout << "\nInitializing Rive renderer..." << std::endl;
        // Initialize GLAD so the renderer's GL function pointers are valid.
        // Without this, `RenderContextGLImpl::MakeContext()` can crash by
        // calling a null function pointer.
        g_eglGetProcAddress = m_drmContext->getProcAddress();
        if (g_eglGetProcAddress == nullptr ||
            gladLoadCustomLoader(gladEglLoader) == 0)
        {
            std::cerr << "Failed to initialize GLAD via eglGetProcAddress"
                      << std::endl;
            return false;
        }
        // Use default renderer settings. With Mesa 26.0+ Panfrost now supports
        // GL_EXT_shader_pixel_local_storage (see Mesa commit 298ad17b81e), so
        // we should NOT force the atomic/MSAA fallback path here.
        RenderContextGLImpl::ContextOptions ctxOpts = {};
        // Optional overrides for debugging/workarounds:
        // - RIVE_DISABLE_PLS=1 forces the old fallback path.
        // - RIVE_DISABLE_FSI=1 disables fragment shader interlock.
        ctxOpts.disablePixelLocalStorage = envFlag("RIVE_DISABLE_PLS", false);
        ctxOpts.disableFragmentShaderInterlock = envFlag("RIVE_DISABLE_FSI", false);
        m_renderContext = RenderContextGLImpl::MakeContext(ctxOpts);

        if (!m_renderContext)
        {
            std::cerr << "Failed to create Rive render context" << std::endl;
            return false;
        }

        std::cout << "Rive renderer initialized" << std::endl;
        
        // Print renderer capabilities and PLS implementation details
        auto renderContextGL = m_renderContext->static_impl_cast<RenderContextGLImpl>();
        const auto& caps = renderContextGL->capabilities();
        std::cout << "\n=== Renderer Capabilities ===" << std::endl;
        std::cout << "EXT_shader_pixel_local_storage: " << (caps.EXT_shader_pixel_local_storage ? "YES" : "NO") << std::endl;
        std::cout << "EXT_shader_pixel_local_storage2: " << (caps.EXT_shader_pixel_local_storage2 ? "YES" : "NO") << std::endl;
        std::cout << "ARM_shader_framebuffer_fetch: " << (caps.ARM_shader_framebuffer_fetch ? "YES" : "NO") << std::endl;
        std::cout << "EXT_shader_framebuffer_fetch: " << (caps.EXT_shader_framebuffer_fetch ? "YES" : "NO") << std::endl;
        std::cout << "ANGLE_shader_pixel_local_storage: " << (caps.ANGLE_shader_pixel_local_storage ? "YES" : "NO") << std::endl;
        std::cout << "ARB_shader_image_load_store: " << (caps.ARB_shader_image_load_store ? "YES" : "NO") << std::endl;
        std::cout << "EXT_color_buffer_integer: " << (caps.EXT_color_buffer_integer ? "YES" : "NO") << std::endl;
        std::cout << "EXT_color_buffer_float: " << (caps.EXT_color_buffer_float ? "YES" : "NO") << std::endl;
        std::cout << "needsFloatingPointTessellationTexture: " << (caps.needsFloatingPointTessellationTexture ? "YES" : "NO") << std::endl;
        std::cout << "Note: If EXT_shader_pixel_local_storage + framebuffer_fetch are both YES, PLS should be active." << std::endl;
        
        // Check for GL errors after renderer creation
        GLenum err = glGetError();
        if (err != GL_NO_ERROR) {
            std::cerr << "GL Error after renderer init: 0x" << std::hex << err << std::dec << std::endl;
        }

        // Default framebuffer (ID 0)
        // Query the sample count from the current framebuffer.
        GLint sampleCount = 0;
        glGetIntegerv(GL_SAMPLES, &sampleCount);
        std::cout << "Default framebuffer: " << m_width << "x" << m_height
                  << " (MSAA: " << sampleCount << "x)" << std::endl;

        // Optional: render at reduced resolution and upscale to the display.
        // This is often the highest-impact FPS knob on embedded GPUs.
        // Priority: explicit dimensions (RIVE_RENDER_WIDTH/HEIGHT) > percentage (RIVE_RENDER_SCALE).
        uint32_t explicitWidth = envU32("RIVE_RENDER_WIDTH", 0);
        uint32_t explicitHeight = envU32("RIVE_RENDER_HEIGHT", 0);
        
        if (explicitWidth > 0 && explicitHeight > 0)
        {
            // Use explicit dimensions
            m_riveWidth = explicitWidth;
            m_riveHeight = explicitHeight;
            std::cout << "Rive render resolution (explicit): " << m_riveWidth << "x"
                      << m_riveHeight << std::endl;

            if (!initOffscreenTarget())
            {
                std::cerr << "Warning: offscreen target init failed; falling back to full-res rendering."
                          << std::endl;
                m_riveWidth = m_width;
                m_riveHeight = m_height;
            }
        }
        else
        {
            // Use percentage-based scale
            float renderScale = envFloat("RIVE_RENDER_SCALE", 100.0f);
            if (renderScale < 10.0f)
            {
                renderScale = 10.0f;
            }
            if (renderScale > 100.0f)
            {
                renderScale = 100.0f;
            }

            if (renderScale < 100.0f)
            {
                m_riveWidth = std::max<uint32_t>(
                    1,
                    static_cast<uint32_t>(m_width * (renderScale / 100.0f)));
                m_riveHeight = std::max<uint32_t>(
                    1,
                    static_cast<uint32_t>(m_height * (renderScale / 100.0f)));
                std::cout << "Rive render scale: " << renderScale << "% => "
                          << m_riveWidth << "x" << m_riveHeight << std::endl;

                if (!initOffscreenTarget())
                {
                    std::cerr << "Warning: offscreen target init failed; falling back to full-res rendering."
                              << std::endl;
                    m_riveWidth = m_width;
                    m_riveHeight = m_height;
                }
            }
        }

        if (!m_offscreenEnabled)
        {
            std::cout << "Creating Rive render target: " << m_width << "x"
                      << m_height << " (default framebuffer)" << std::endl;
            m_renderTarget = make_rcp<FramebufferRenderTargetGL>(
                m_width,
                m_height,
                0, // default framebuffer
                sampleCount);
        }

        // Load .riv file
        std::cout << "\nLoading Rive file: " << rivPath << std::endl;
        if (!loadRiveFile(rivPath))
        {
            return false;
        }

        // Create the renderer once and reuse it every frame. This avoids
        // per-frame heap allocations and initialization overhead.
        m_renderer = std::make_unique<RiveRenderer>(m_renderContext.get());

        std::cout << "\n=== Initialization Complete ===" << std::endl;
        std::cout << "Display: " << m_width << "x" << m_height << std::endl;
        std::cout << "Artboard: " << m_artboard->name() << std::endl;
        std::cout << "Size: " << m_artboard->width() << "x"
                  << m_artboard->height() << std::endl;

        // Print scene information (avoid dynamic_cast RTTI dependency).
        if (m_scene)
        {
            std::cout << "Scene: " << m_scene->name() << std::endl;
        }

        return true;
    }

    bool loadRiveFile(const char* path)
    {
        // Read file into memory
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open())
        {
            std::cerr << "Failed to open file: " << path << std::endl;
            return false;
        }

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<uint8_t> buffer(size);
        if (!file.read(reinterpret_cast<char*>(buffer.data()), size))
        {
            std::cerr << "Failed to read file" << std::endl;
            return false;
        }

        std::cout << "Read " << size << " bytes from file" << std::endl;

        // Import Rive file
        m_file = File::import(buffer, m_renderContext.get());
        if (!m_file)
        {
            std::cerr << "Failed to import Rive file" << std::endl;
            return false;
        }

        // Get default artboard
        m_artboard = m_file->artboardDefault();
        if (!m_artboard)
        {
            std::cerr << "No artboard found in file" << std::endl;
            return false;
        }

        // Try to get a state machine or animation
        m_scene = m_artboard->defaultStateMachine();
        if (!m_scene)
        {
            std::cout << "No state machine found, trying animation..."
                      << std::endl;
            m_scene = m_artboard->animationAt(0);
        }

        if (!m_scene)
        {
            std::cout << "Warning: No animation or state machine found"
                      << std::endl;
        }

        return true;
    }

    void run()
    {
        std::cout << "\n=== Starting Render Loop ===" << std::endl;
        std::cout << "Press Ctrl+C to quit" << std::endl;

        auto lastTime = std::chrono::high_resolution_clock::now();
        
        while (g_running)
        {
            auto currentTime = std::chrono::high_resolution_clock::now();
            std::chrono::duration<float> elapsed = currentTime - lastTime;
            float deltaTime = elapsed.count();
            lastTime = currentTime;

            // Advance animation
            if (m_scene)
            {
                m_scene->advanceAndApply(deltaTime);
            }

            // Render frame
            renderFrame();
        }

        std::cout << "\n=== Shutting Down ===" << std::endl;
    }

private:
    bool initOffscreenTarget()
    {
        // Create an offscreen FBO with a color texture + stencil renderbuffer.
        // Rive uses stencil for clipping, so we provide an 8-bit stencil buffer.
        if (m_offscreenFbo != 0)
        {
            // Already initialized.
            std::cout << "Offscreen target already initialized (FBO " << m_offscreenFbo << ")" << std::endl;
            return true;
        }

        std::cout << "Creating offscreen target: " << m_riveWidth << "×" << m_riveHeight << std::endl;

        glGenFramebuffers(1, &m_offscreenFbo);
        glBindFramebuffer(GL_FRAMEBUFFER, m_offscreenFbo);

        glGenTextures(1, &m_offscreenColorTex);
        glBindTexture(GL_TEXTURE_2D, m_offscreenColorTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     GL_RGBA,  // Internal format (GLES3 compatible)
                     static_cast<GLsizei>(m_riveWidth),
                     static_cast<GLsizei>(m_riveHeight),
                     0,
                     GL_RGBA,  // Format
                     GL_UNSIGNED_BYTE,  // Type
                     nullptr);
        glFramebufferTexture2D(GL_FRAMEBUFFER,
                               GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D,
                               m_offscreenColorTex,
                               0);

        GLenum glErr = glGetError();
        if (glErr != GL_NO_ERROR)
        {
            std::cerr << "GL Error after color attachment: 0x" << std::hex << glErr << std::dec << std::endl;
        }

        glGenRenderbuffers(1, &m_offscreenStencilRb);
        glBindRenderbuffer(GL_RENDERBUFFER, m_offscreenStencilRb);
        glRenderbufferStorage(GL_RENDERBUFFER,
                              GL_STENCIL_INDEX8,
                              static_cast<GLsizei>(m_riveWidth),
                              static_cast<GLsizei>(m_riveHeight));
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                                  GL_STENCIL_ATTACHMENT,
                                  GL_RENDERBUFFER,
                                  m_offscreenStencilRb);

        glErr = glGetError();
        if (glErr != GL_NO_ERROR)
        {
            std::cerr << "GL Error after stencil attachment: 0x" << std::hex << glErr << std::dec << std::endl;
        }

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            std::cerr << "Offscreen FBO incomplete: 0x" << std::hex << status
                      << std::dec << std::endl;
            m_offscreenEnabled = false;
            return false;
        }

        std::cout << "Offscreen FBO created successfully (ID " << m_offscreenFbo << ")" << std::endl;

        // Create a Rive render target backed by our offscreen FBO.
        m_renderTarget = make_rcp<FramebufferRenderTargetGL>(
            m_riveWidth,
            m_riveHeight,
            static_cast<uint32_t>(m_offscreenFbo),
            0 /* sampleCount */);

        m_offscreenEnabled = true;
        return true;
    }

    void renderFrame()
    {
        // FPS tracking
        m_frameCount++;
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastFpsReport).count();
        if (elapsed >= 1000) { // Report every second
            double fps = (m_frameCount * 1000.0) / elapsed;
            std::cout << "FPS: " << std::fixed << std::setprecision(1) << fps 
                      << " (" << m_frameCount << " frames in " << elapsed << "ms)";
            if (m_offscreenEnabled)
            {
                std::cout << " [offscreen: " << m_riveWidth << "×" << m_riveHeight << "]";
            }
            std::cout << std::endl;
            m_frameCount = 0;
            m_lastFpsReport = now;
        }
        
        m_drmContext->beginFrame();

        // Set viewport (Rive will handle clearing)
        glViewport(0, 0, m_riveWidth, m_riveHeight);

        if (!m_artboard)
        {
            m_drmContext->swapBuffers();
            return;
        }
        if (!m_renderer)
        {
            // Should not happen (created in initialize), but be defensive.
            m_renderer = std::make_unique<RiveRenderer>(m_renderContext.get());
        }

        // Setup frame.
        // By default we let Rive choose the best interlock mode.
        // Optional debug overrides:
        // - RIVE_MSAA_SAMPLES=4 forces MSAA interlock mode (often best for correctness).
        // - RIVE_DISABLE_RO=1 disables raster ordering (may force atomic/clockwise-atomic).
        // - RIVE_CLOCKWISE=1 enables clockwise fill override (workaround for winding issues).
        uint32_t forcedMSAA = envU32("RIVE_MSAA_SAMPLES", 0);
        
        RenderContext::FrameDescriptor frameDesc = {
            .renderTargetWidth = m_riveWidth,
            .renderTargetHeight = m_riveHeight,
            .clearColor = 0xff404040, // Dark gray background
            .msaaSampleCount = forcedMSAA,
            .disableRasterOrdering = envFlag("RIVE_DISABLE_RO", false),
            .clockwiseFillOverride = envFlag("RIVE_CLOCKWISE", false),
        };

        // Begin frame
        auto renderContextGL = m_renderContext->static_impl_cast<RenderContextGLImpl>();
        renderContextGL->invalidateGLState();
        m_renderContext->beginFrame(frameDesc);

        // Calculate alignment to fit artboard in display
        float artboardWidth = m_artboard->width();
        float artboardHeight = m_artboard->height();

        if (artboardWidth <= 0 || artboardHeight <= 0)
        {
            artboardWidth = m_riveWidth;
            artboardHeight = m_riveHeight;
        }

        AABB displayBounds(0, 0, m_riveWidth, m_riveHeight);
        AABB artboardBounds(0, 0, artboardWidth, artboardHeight);

        // Align artboard to center of display with contain fit
        m_renderer->save();
        m_renderer->align(Fit::contain,
                          Alignment::center,
                          displayBounds,
                          artboardBounds);

        // Draw artboard
        m_artboard->draw(m_renderer.get());

        m_renderer->restore();

        // Flush renderer - submit GPU commands
        m_renderContext->flush({.renderTarget = m_renderTarget.get()});

        // If rendering offscreen, blit to the display framebuffer (0).
        if (m_offscreenEnabled && m_offscreenFbo != 0)
        {
            // Determine blit destination rectangle.
            // RIVE_RENDER_STRETCH=1: stretch to fill display (distorts if aspect differs)
            // RIVE_RENDER_STRETCH=0 (default): 1:1 pixel mapping, centered
            int dstX0, dstY0, dstX1, dstY1;
            
            const bool stretch = envFlag("RIVE_RENDER_STRETCH", false);
            if (stretch)
            {
                // Stretch to fill entire display
                dstX0 = 0;
                dstY0 = 0;
                dstX1 = static_cast<GLint>(m_width);
                dstY1 = static_cast<GLint>(m_height);
            }
            else
            {
                // 1:1 pixel mapping, centered on display
                int xOffset = (static_cast<int>(m_width) - static_cast<int>(m_riveWidth)) / 2;
                int yOffset = (static_cast<int>(m_height) - static_cast<int>(m_riveHeight)) / 2;
                dstX0 = xOffset;
                dstY0 = yOffset;
                dstX1 = xOffset + static_cast<int>(m_riveWidth);
                dstY1 = yOffset + static_cast<int>(m_riveHeight);
            }
            
            // Clear the display framebuffer so edges don't have stale content when we're
            // blitting only a sub-rect (i.e. not stretching).
            //
            // For performance experiments, you can disable per-frame clears:
            //   RIVE_CLEAR_EVERY_FRAME=0
            //
            // When stretching to the full display, the blit covers the entire surface,
            // so a clear is unnecessary.
            const bool clearEveryFrame = envFlag("RIVE_CLEAR_EVERY_FRAME", true);
            if (!stretch && (clearEveryFrame || !m_clearedDisplayOnce))
            {
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
                glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT);
                m_clearedDisplayOnce = true;
            }
            
            // Blit from offscreen to display
            glBindFramebuffer(GL_READ_FRAMEBUFFER, m_offscreenFbo);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            glBlitFramebuffer(0,
                              0,
                              static_cast<GLint>(m_riveWidth),
                              static_cast<GLint>(m_riveHeight),
                              dstX0,
                              dstY0,
                              dstX1,
                              dstY1,
                              GL_COLOR_BUFFER_BIT,
                              GL_LINEAR);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        }

        // Swap buffers
        m_drmContext->swapBuffers();
    }

private:
    std::unique_ptr<rive_rk3566::DRMEGLContext> m_drmContext;
    std::unique_ptr<RenderContext> m_renderContext;
    std::unique_ptr<RiveRenderer> m_renderer;
    rcp<RenderTarget> m_renderTarget;
    rcp<File> m_file;
    std::unique_ptr<ArtboardInstance> m_artboard;
    std::unique_ptr<Scene> m_scene;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint32_t m_riveWidth = 0;
    uint32_t m_riveHeight = 0;

    bool m_offscreenEnabled = false;
    GLuint m_offscreenFbo = 0;
    GLuint m_offscreenColorTex = 0;
    GLuint m_offscreenStencilRb = 0;
    
    // FPS tracking
    std::chrono::steady_clock::time_point m_lastFpsReport;
    int m_frameCount = 0;

    // Display clear control (used when blitting a sub-rect to the onscreen FB).
    bool m_clearedDisplayOnce = false;
};

void printUsage(const char* progName)
{
    std::cout << "Usage: " << progName << " <path-to-riv-file>" << std::endl;
    std::cout << std::endl;
    std::cout << "Example:" << std::endl;
    std::cout << "  " << progName << " /home/orangepi/animation.riv" << std::endl;
    std::cout << std::endl;
    std::cout << "Note: You may need to run as root or be in 'video' group for DRM access:"
              << std::endl;
    std::cout << "  sudo " << progName << " animation.riv" << std::endl;
    std::cout << "  or" << std::endl;
    std::cout << "  sudo usermod -a -G video $USER" << std::endl;
}

int main(int argc, char* argv[])
{
    // Parse arguments
    if (argc < 2)
    {
        std::cerr << "Error: No .riv file specified" << std::endl;
        printUsage(argv[0]);
        return 1;
    }

    const char* rivPath = argv[1];

    // Check if file exists
    std::ifstream testFile(rivPath);
    if (!testFile.good())
    {
        std::cerr << "Error: File not found or cannot be read: " << rivPath
                  << std::endl;
        return 1;
    }
    testFile.close();

    // Setup signal handlers for graceful shutdown
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // Create and run player
    RK3566Player player;
    if (!player.initialize(rivPath))
    {
        std::cerr << "Failed to initialize player" << std::endl;
        return 1;
    }

    player.run();

    return 0;
}

