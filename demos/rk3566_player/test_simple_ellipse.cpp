/*
 * Simple ellipse rendering test
 * Tests if basic ellipse geometry is generated correctly
 */

#include "drm_egl_context.h"
#include "rive/artboard.hpp"
#include "rive/file.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include "rive/renderer/gl/render_context_gl_impl.hpp"
#include "rive/renderer/gl/render_target_gl.hpp"
#include "rive/math/aabb.hpp"
#include "rive/shapes/paint/color.hpp"

#include <iostream>
#include <memory>
#include <signal.h>

using namespace rive;
using namespace rive::gpu;

extern "C"
{
    using GLADapiproc = void (*)();
    using GLADloadfunc = GLADapiproc (*)(const char* name);
    int gladLoadCustomLoader(GLADloadfunc);
}

static void* (*g_eglGetProcAddress)(const char*) = nullptr;
static GLADapiproc gladEglLoader(const char* name)
{
    return (GLADapiproc)(g_eglGetProcAddress ? g_eglGetProcAddress(name) : nullptr);
}

static volatile bool g_running = true;

void signalHandler(int signum)
{
    std::cout << "\nInterrupt signal received.\n";
    g_running = false;
}

int main(int argc, char* argv[])
{
    std::cout << "=== Simple Ellipse Rendering Test ===" << std::endl;

    // Initialize DRM/EGL
    auto drmContext = std::make_unique<rive_rk3566::DRMEGLContext>();
    if (!drmContext->initialize())
    {
        std::cerr << "Failed to initialize DRM/EGL" << std::endl;
        return 1;
    }

    uint32_t width = drmContext->width();
    uint32_t height = drmContext->height();
    std::cout << "Display: " << width << "x" << height << std::endl;

    // Initialize GLAD
    g_eglGetProcAddress = drmContext->getProcAddress();
    if (!g_eglGetProcAddress || gladLoadCustomLoader(gladEglLoader) == 0)
    {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return 1;
    }

    // Create Rive renderer
    RenderContextGLImpl::ContextOptions ctxOpts = {};
    auto renderContext = RenderContextGLImpl::MakeContext(ctxOpts);
    if (!renderContext)
    {
        std::cerr << "Failed to create Rive render context" << std::endl;
        return 1;
    }

    std::cout << "Renderer initialized" << std::endl;

    // Print capabilities
    auto renderContextGL = renderContext->static_impl_cast<RenderContextGLImpl>();
    const auto& caps = renderContextGL->capabilities();
    std::cout << "EXT_shader_pixel_local_storage: " << (caps.EXT_shader_pixel_local_storage ? "YES" : "NO") << std::endl;
    std::cout << "EXT_shader_framebuffer_fetch: " << (caps.EXT_shader_framebuffer_fetch ? "YES" : "NO") << std::endl;

    // Create render target
    GLint sampleCount = 0;
    glGetIntegerv(GL_SAMPLES, &sampleCount);
    auto renderTarget = make_rcp<FramebufferRenderTargetGL>(width, height, 0, sampleCount);

    // Setup signal handler
    signal(SIGINT, signalHandler);

    std::cout << "\n=== Drawing Simple Ellipses ===" << std::endl;
    std::cout << "Press Ctrl+C to quit" << std::endl;

    int frameCount = 0;
    while (g_running && frameCount < 100)
    {
        drmContext->beginFrame();
        glViewport(0, 0, width, height);

        // Create renderer for this frame
        auto renderer = std::make_unique<RiveRenderer>(renderContext.get());

        // Setup frame
        RenderContext::FrameDescriptor frameDesc = {
            .renderTargetWidth = width,
            .renderTargetHeight = height,
            .clearColor = 0xff202020, // Dark gray
        };

        renderContextGL->invalidateGLState();
        renderContext->beginFrame(frameDesc);

        // Draw three ellipses of different sizes
        renderer->save();

        // Ellipse 1: Large (should be easy to see if broken)
        auto path1 = renderer->makeRenderPath(FillRule::nonZero);
        path1->addRenderPath(RenderPath::Make<rive::RawPath>(), Mat2D());
        // TODO: Need to create actual ellipse path data
        
        std::cout << "Frame " << frameCount << std::endl;

        renderer->restore();

        // Flush
        renderContext->flush({.renderTarget = renderTarget.get()});

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glFinish();

        drmContext->swapBuffers();

        frameCount++;
        usleep(33000); // ~30 FPS
    }

    std::cout << "\n=== Test Complete ===" << std::endl;
    std::cout << "Rendered " << frameCount << " frames" << std::endl;

    return 0;
}

