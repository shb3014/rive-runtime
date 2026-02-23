#include "drm_egl_context.h"
#include <GLES3/gl3.h>
#include <cstdio>
#include <signal.h>

static volatile bool g_running = true;
void sigHandler(int) { g_running = false; }

int main()
{
    signal(SIGINT, sigHandler);
    signal(SIGTERM, sigHandler);

    auto ctx = std::make_unique<rive_rk3566::DRMEGLContext>();
    if (!ctx->initialize())
    {
        fprintf(stderr, "EGL init failed\n");
        return 1;
    }

    fprintf(stderr, "Display: %ux%u\n", ctx->width(), ctx->height());
    int frames = 0;

    while (g_running)
    {
        ctx->beginFrame();
        glViewport(0, 0, ctx->width(), ctx->height());
        glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        for (GLenum e = glGetError(); e != GL_NO_ERROR; e = glGetError())
            fprintf(stderr, "[GL] error before swap: 0x%x\n", e);

        ctx->swapBuffers();

        for (GLenum e = glGetError(); e != GL_NO_ERROR; e = glGetError())
            fprintf(stderr, "[GL] error after swap: 0x%x\n", e);

        ++frames;
    }

    fprintf(stderr, "Rendered %d frames, no Rive\n", frames);
    return 0;
}
