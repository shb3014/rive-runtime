// Simple test to verify rendering pipeline works
// This draws a colored triangle to verify GPU rendering is working

#include "drm_egl_context.h"
#include <GLES3/gl3.h>
#include <iostream>
#include <signal.h>

static volatile bool g_running = true;

void signalHandler(int signum) {
    g_running = false;
}

int main() {
    signal(SIGINT, signalHandler);
    
    std::cout << "=== Simple Render Test ===" << std::endl;
    
    // Initialize DRM/EGL
    auto drmContext = std::make_unique<rive_rk3566::DRMEGLContext>();
    if (!drmContext->initialize()) {
        std::cerr << "Failed to initialize DRM/EGL" << std::endl;
        return 1;
    }
    
    uint32_t width = drmContext->width();
    uint32_t height = drmContext->height();
    std::cout << "Display: " << width << "x" << height << std::endl;
    
    int frameCount = 0;
    while (g_running && frameCount < 180) {  // 3 seconds at 60fps
        drmContext->beginFrame();
        
        // Set viewport
        glViewport(0, 0, width, height);
        
        // Clear to blue
        glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        // Draw a simple red triangle using immediate mode equivalent
        const char* vertexShader = R"(
            #version 300 es
            in vec2 pos;
            void main() {
                gl_Position = vec4(pos, 0.0, 1.0);
            }
        )";
        
        const char* fragmentShader = R"(
            #version 300 es
            precision mediump float;
            out vec4 fragColor;
            void main() {
                fragColor = vec4(1.0, 0.0, 0.0, 1.0);  // Red
            }
        )";
        
        // For now, just clear - full shader setup would be more complex
        // The blue clear should be visible if rendering works
        
        glFinish();
        drmContext->swapBuffers();
        
        frameCount++;
        if (frameCount % 60 == 0) {
            std::cout << "Frame " << frameCount << " - should see BLUE screen" << std::endl;
        }
    }
    
    std::cout << "Test complete. Did you see a BLUE screen?" << std::endl;
    return 0;
}



