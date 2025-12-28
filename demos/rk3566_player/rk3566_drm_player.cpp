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
#include <signal.h>
#include <cstring>

using namespace rive;
using namespace rive::gpu;

// Global flag for graceful shutdown
static volatile bool g_running = true;

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
        // Use fallback rendering path for Panfrost compatibility
        // Panfrost doesn't fully support all PLS features yet
        m_renderContext = RenderContextGLImpl::MakeContext(
            {.disablePixelLocalStorage = true,
             .disableFragmentShaderInterlock = true});

        if (!m_renderContext)
        {
            std::cerr << "Failed to create Rive render context" << std::endl;
            return false;
        }

        std::cout << "Rive renderer initialized" << std::endl;
        
        // Check for GL errors after renderer creation
        GLenum err = glGetError();
        if (err != GL_NO_ERROR) {
            std::cerr << "GL Error after renderer init: 0x" << std::hex << err << std::dec << std::endl;
        }

        // Create render target for the default framebuffer (ID 0)
        // Query the sample count from the current framebuffer
        GLint sampleCount = 0;
        glGetIntegerv(GL_SAMPLES, &sampleCount);
        std::cout << "Creating render target: " << m_width << "x" << m_height 
                  << " (MSAA: " << sampleCount << "x)" << std::endl;
        m_renderTarget = make_rcp<FramebufferRenderTargetGL>(
            m_width,
            m_height,
            0,  // Use framebuffer 0 (default framebuffer)
            sampleCount);

        // Load .riv file
        std::cout << "\nLoading Rive file: " << rivPath << std::endl;
        if (!loadRiveFile(rivPath))
        {
            return false;
        }

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
        int frameCount = 0;
        auto fpsTime = lastTime;

        fprintf(stderr, "[DEBUG] About to enter while loop\n"); fflush(stderr);
        
        while (g_running)
        {
            fprintf(stderr, "[DEBUG] Loop start\n"); fflush(stderr);
            auto currentTime = std::chrono::high_resolution_clock::now();
            std::chrono::duration<float> elapsed = currentTime - lastTime;
            float deltaTime = elapsed.count();
            lastTime = currentTime;

            // Advance animation
            fprintf(stderr, "[DEBUG] Before scene advance\n"); fflush(stderr);
            if (m_scene)
            {
                fprintf(stderr, "[DEBUG] Calling advanceAndApply\n"); fflush(stderr);
                m_scene->advanceAndApply(deltaTime);
                fprintf(stderr, "[DEBUG] After advanceAndApply\n"); fflush(stderr);
            }

            // Render frame
            fprintf(stderr, "[DEBUG] Before renderFrame\n"); fflush(stderr);
            renderFrame();
            fprintf(stderr, "[DEBUG] After renderFrame\n"); fflush(stderr);

            // Calculate FPS
            frameCount++;
            std::chrono::duration<float> fpsElapsed = currentTime - fpsTime;
            if (fpsElapsed.count() >= 2.0f)
            {
                float fps = frameCount / fpsElapsed.count();
                std::cout << "FPS: " << fps << " (" << (1000.0f / fps)
                          << " ms/frame)" << std::endl;
                frameCount = 0;
                fpsTime = currentTime;
            }
        }

        std::cout << "\n=== Shutting Down ===" << std::endl;
    }

private:
    void renderFrame()
    {
        std::cout << "[DEBUG] renderFrame: start" << std::endl;
        m_drmContext->beginFrame();
        std::cout << "[DEBUG] renderFrame: after beginFrame" << std::endl;

        // Set viewport (Rive will handle clearing)
        glViewport(0, 0, m_width, m_height);
        std::cout << "[DEBUG] renderFrame: after viewport setup" << std::endl;

        if (!m_artboard)
        {
            m_drmContext->swapBuffers();
            return;
        }

        // Create renderer
        std::cout << "[DEBUG] renderFrame: creating renderer" << std::endl;
        auto renderer = std::make_unique<RiveRenderer>(m_renderContext.get());
        std::cout << "[DEBUG] renderFrame: renderer created" << std::endl;

        // Setup frame - use atomic mode (most compatible for Panfrost)
        RenderContext::FrameDescriptor frameDesc = {
            .renderTargetWidth = m_width,
            .renderTargetHeight = m_height,
            .clearColor = 0xff404040, // Dark gray background
            .msaaSampleCount = 1, // Atomic mode requires sample count = 1
            .disableRasterOrdering = true, // Force atomic/MSAA path
            .clockwiseFillOverride = true, // Emulate clockwise atomic mode
        };

        // Begin frame
        std::cout << "[DEBUG] renderFrame: calling renderContext->beginFrame" << std::endl;
        auto renderContextGL = m_renderContext->static_impl_cast<RenderContextGLImpl>();
        renderContextGL->invalidateGLState();  // Reset GL state before rendering
        m_renderContext->beginFrame(frameDesc);
        std::cout << "[DEBUG] renderFrame: after renderContext->beginFrame" << std::endl;

        // Calculate alignment to fit artboard in display
        float artboardWidth = m_artboard->width();
        float artboardHeight = m_artboard->height();

        if (artboardWidth <= 0 || artboardHeight <= 0)
        {
            artboardWidth = m_width;
            artboardHeight = m_height;
        }

        AABB displayBounds(0, 0, m_width, m_height);
        AABB artboardBounds(0, 0, artboardWidth, artboardHeight);

        // Align artboard to center of display with contain fit
        std::cout << "[DEBUG] renderFrame: setting up alignment" << std::endl;
        renderer->save();
        renderer->align(Fit::contain,
                        Alignment::center,
                        displayBounds,
                        artboardBounds);
        std::cout << "[DEBUG] renderFrame: after alignment" << std::endl;

        // Draw artboard
        std::cout << "[DEBUG] renderFrame: drawing artboard" << std::endl;
        m_artboard->draw(renderer.get());
        std::cout << "[DEBUG] renderFrame: after draw" << std::endl;
        
        // Check for GL errors after draw
        GLenum drawErr = glGetError();
        if (drawErr != GL_NO_ERROR) {
            std::cerr << "[ERROR] GL Error after draw: 0x" << std::hex << drawErr << std::dec << std::endl;
        }

        renderer->restore();

        // Flush renderer - submit GPU commands
        std::cout << "[DEBUG] renderFrame: flushing" << std::endl;
        m_renderContext->flush({.renderTarget = m_renderTarget.get()});
        std::cout << "[DEBUG] renderFrame: after flush" << std::endl;

        // Bind default framebuffer and complete rendering
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glFinish();
        std::cout << "[DEBUG] renderFrame: after glFinish" << std::endl;

        // Swap buffers
        std::cout << "[DEBUG] renderFrame: swapping buffers" << std::endl;
        m_drmContext->swapBuffers();
        std::cout << "[DEBUG] renderFrame: complete" << std::endl;
    }

private:
    std::unique_ptr<rive_rk3566::DRMEGLContext> m_drmContext;
    std::unique_ptr<RenderContext> m_renderContext;
    rcp<RenderTarget> m_renderTarget;
    rcp<File> m_file;
    std::unique_ptr<ArtboardInstance> m_artboard;
    std::unique_ptr<Scene> m_scene;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
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

