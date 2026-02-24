/*
 * Avatar Eye Tracker Demo
 * Combines SoulCam human detection with Rive avatar animation.
 * The avatar's eyes continuously track detected humans via Joystick control.
 *
 * Architecture:
 *   avatar.riv has 2 artboards: "Artboard" (wrapper) and "Rive Avatar Rubick".
 *   "Rive Avatar Rubick" is nested inside "Artboard" and contains:
 *     - A Joystick (428x378) that drives x/y blend animations
 *     - Joystick x/y range is [-1, 1], mapped to animation blend
 *     - Animations: kedip (blink), kepala (head), x, y, hair, sWING, Idle, etc.
 *
 *   We bind to /tmp/soulcam_scene.sock to receive SoulCam detection JSON,
 *   then map detected person's position to continuous joystick x/y values
 *   with smooth interpolation for natural eye movement.
 */

#include "drm_egl_context.h"
#include "rive/artboard.hpp"
#include "rive/file.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include "rive/renderer/gl/render_context_gl_impl.hpp"
#include "rive/renderer/gl/render_target_gl.hpp"
#include "rive/math/aabb.hpp"
#include "rive/node.hpp"
#include "rive/joystick.hpp"
#include "rive/nested_artboard.hpp"
#include <GLES3/gl3.h>

extern "C"
{
    using GLADapiproc = void (*)();
    using GLADloadfunc = GLADapiproc (*)(const char* name);
    int gladLoadCustomLoader(GLADloadfunc);
}

#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include <memory>
#include <iomanip>
#include <signal.h>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sstream>
#include <cmath>
#include <algorithm>

using namespace rive;
using namespace rive::gpu;

static volatile bool g_running = true;
static void* (*g_eglGetProcAddress)(const char*) = nullptr;

static GLADapiproc gladEglLoader(const char* name)
{
    return (GLADapiproc)(g_eglGetProcAddress ? g_eglGetProcAddress(name)
                                             : nullptr);
}

void signalHandler(int signum)
{
    std::cout << "\nSignal " << signum << " received, shutting down.\n";
    g_running = false;
}

// ---------------------------------------------------------------------------
// JSON parser for SoulCam detection data
// ---------------------------------------------------------------------------
struct Detection
{
    int cls_id = 0;
    std::string label;
    float conf = 0;
    struct
    {
        int left = 0, top = 0, right = 0, bottom = 0;
    } box;
};

struct DetectionData
{
    int count = 0;
    std::vector<Detection> objects;
};

bool parseDetectionJSON(const std::string& json, DetectionData& data)
{
    data.count = 0;
    data.objects.clear();

    size_t countPos = json.find("\"count\":");
    if (countPos != std::string::npos)
    {
        size_t numStart = json.find_first_of("0123456789", countPos);
        if (numStart != std::string::npos)
            data.count = std::stoi(json.substr(numStart));
    }

    size_t objsPos = json.find("\"objects\":");
    if (objsPos == std::string::npos)
        return false;
    size_t bracketPos = json.find("[", objsPos + 10);
    if (bracketPos == std::string::npos)
        return false;

    size_t pos = bracketPos + 1;
    while (pos < json.length())
    {
        size_t objStart = json.find("{", pos);
        if (objStart == std::string::npos)
            break;

        int depth = 0;
        size_t objEnd = objStart;
        for (size_t i = objStart; i < json.length(); i++)
        {
            if (json[i] == '{')
                depth++;
            else if (json[i] == '}')
            {
                depth--;
                if (depth == 0)
                {
                    objEnd = i;
                    break;
                }
            }
        }
        if (objEnd == objStart)
            break;

        std::string objStr = json.substr(objStart, objEnd - objStart + 1);
        Detection det;

        auto findValue = [&](const std::string& s, const char* key,
                             size_t from) -> size_t {
            std::string k = std::string("\"") + key + "\"";
            size_t p = s.find(k, from);
            if (p == std::string::npos)
                return std::string::npos;
            p = s.find(':', p + k.size());
            if (p == std::string::npos)
                return std::string::npos;
            p++;
            while (p < s.size() && (s[p] == ' ' || s[p] == '\t'))
                p++;
            return p;
        };

        size_t vp = findValue(objStr, "label", 0);
        if (vp != std::string::npos && vp < objStr.size() &&
            objStr[vp] == '"')
        {
            size_t labelStart = vp + 1;
            size_t labelEnd = objStr.find('"', labelStart);
            if (labelEnd != std::string::npos)
                det.label = objStr.substr(labelStart, labelEnd - labelStart);
        }

        vp = findValue(objStr, "conf", 0);
        if (vp != std::string::npos)
            det.conf = std::stof(objStr.substr(vp));

        size_t boxPos = findValue(objStr, "box", 0);
        if (boxPos != std::string::npos)
        {
            auto parseBoxField = [&](const char* field, int& out) {
                size_t fp = findValue(objStr, field, boxPos);
                if (fp != std::string::npos)
                    out = std::stoi(objStr.substr(fp));
            };
            parseBoxField("left", det.box.left);
            parseBoxField("top", det.box.top);
            parseBoxField("right", det.box.right);
            parseBoxField("bottom", det.box.bottom);
        }

        data.objects.push_back(det);
        pos = objEnd + 1;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Artboard inspector (reused from owl demo)
// ---------------------------------------------------------------------------
static void inspectSingleArtboard(Artboard* ab, const std::string& prefix)
{
    std::cout << prefix << "Name: " << ab->name() << std::endl;
    std::cout << prefix << "Size: " << ab->width() << "x" << ab->height()
              << std::endl;

    std::cout << prefix << "Animations (" << ab->animationCount()
              << "):" << std::endl;
    for (size_t i = 0; i < ab->animationCount(); i++)
        std::cout << prefix << "  [" << i << "] " << ab->animationNameAt(i)
                  << std::endl;

    std::cout << prefix << "State Machines (" << ab->stateMachineCount()
              << "):" << std::endl;
    for (size_t i = 0; i < ab->stateMachineCount(); i++)
        std::cout << prefix << "  [" << i << "] "
                  << ab->stateMachineNameAt(i) << std::endl;

    auto joysticks = ab->find<Joystick>();
    std::cout << prefix << "Joysticks (" << joysticks.size()
              << "):" << std::endl;
    for (auto* j : joysticks)
        std::cout << prefix << "  - \"" << j->name() << "\" x=" << j->x()
                  << " y=" << j->y() << " w=" << j->width()
                  << " h=" << j->height() << std::endl;

    auto nested = ab->nestedArtboards();
    std::cout << prefix << "Nested Artboards (" << nested.size()
              << "):" << std::endl;
    for (auto* na : nested)
    {
        std::cout << prefix << "  - \"" << na->name() << "\"" << std::endl;
        auto* inst = na->artboardInstance();
        if (inst)
        {
            std::cout << prefix << "    [instance]:" << std::endl;
            inspectSingleArtboard(inst, prefix + "    ");
        }
    }
}

static void inspectFile(File* file)
{
    std::cout << "\n=== File Inspection ===" << std::endl;
    std::cout << "Total artboards in file: " << file->artboardCount()
              << std::endl;
    for (size_t i = 0; i < file->artboardCount(); i++)
    {
        auto* ab = file->artboard(i);
        std::cout << "  [" << i << "] \"" << (ab ? ab->name() : "null")
                  << "\"" << std::endl;
    }

    for (size_t i = 0; i < file->artboardCount(); i++)
    {
        auto inst = file->artboardAt(i);
        if (!inst)
            continue;
        std::cout << "\n--- Artboard " << i << " ---" << std::endl;
        inspectSingleArtboard(inst.get(), "  ");

        for (size_t s = 0; s < inst->stateMachineCount(); s++)
        {
            auto sm = inst->stateMachineAt(s);
            if (sm)
            {
                std::cout << "  State Machine \"" << sm->name()
                          << "\" inputs=" << sm->inputCount() << std::endl;
                for (size_t j = 0; j < sm->inputCount(); j++)
                {
                    auto* inp = sm->input(j);
                    std::cout << "    [" << j << "] \"" << inp->name() << "\""
                              << std::endl;
                }
            }
        }
    }

    std::cout << "\n=== End Inspection ===" << std::endl;
}

// ---------------------------------------------------------------------------
// Main demo class
// ---------------------------------------------------------------------------
class AvatarTrackerDemo
{
public:
    AvatarTrackerDemo() = default;
    ~AvatarTrackerDemo()
    {
        if (m_sceneSocket >= 0)
            close(m_sceneSocket);
    }

    void setRenderSize(uint32_t w, uint32_t h) { m_renderWidth = w; m_renderHeight = h; }
    void setShowTiming(bool v) { m_showTiming = v; }

    bool initialize(const char* rivPath, const char* socketPath,
                    bool inspectOnly)
    {
        std::cout << "=== Avatar Eye Tracker Demo ===" << std::endl;

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

        g_eglGetProcAddress = m_drmContext->getProcAddress();
        if (!g_eglGetProcAddress ||
            gladLoadCustomLoader(gladEglLoader) == 0)
        {
            std::cerr << "Failed to initialize GLAD" << std::endl;
            return false;
        }

        RenderContextGLImpl::ContextOptions ctxOpts = {};
        m_renderContext = RenderContextGLImpl::MakeContext(ctxOpts);
        if (!m_renderContext)
        {
            std::cerr << "Failed to create Rive render context" << std::endl;
            return false;
        }

        // Offscreen FBO for Rive rendering at reduced resolution
        glGenFramebuffers(1, &m_fbo);
        glGenTextures(1, &m_fboTex);
        glBindTexture(GL_TEXTURE_2D, m_fboTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_renderWidth, m_renderHeight,
                     0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, m_fboTex, 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            std::cerr << "FBO incomplete!" << std::endl;
            return false;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        m_renderTarget = make_rcp<FramebufferRenderTargetGL>(
            m_renderWidth, m_renderHeight, m_fbo, 0);

        std::cout << "Loading Rive file: " << rivPath << std::endl;
        if (!loadRiveFile(rivPath))
            return false;

        if (inspectOnly)
        {
            inspectFile(m_file.get());
            return false;
        }

        m_renderer = std::make_unique<RiveRenderer>(m_renderContext.get());

        findJoystick();

        std::cout << "Setting up scene socket: " << socketPath << std::endl;
        if (!connectSceneSocket(socketPath))
        {
            std::cerr << "Warning: Failed to set up scene socket." << std::endl;
        }

        std::cout << "\n=== Initialization Complete ===" << std::endl;
        std::cout << "Display: " << m_width << "x" << m_height << std::endl;
        std::cout << "Artboard: " << m_artboard->name()
                  << " (" << m_artboard->width() << "x"
                  << m_artboard->height() << ")" << std::endl;
        std::cout << "Eye control: "
                  << (m_joystick ? "Joystick (continuous x/y)"
                                : "NONE — tracking disabled")
                  << std::endl;

        return true;
    }

    bool loadRiveFile(const char* path)
    {
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

        m_file = File::import(buffer, m_renderContext.get());
        if (!m_file)
        {
            std::cerr << "Failed to import Rive file" << std::endl;
            return false;
        }

        m_artboard = m_file->artboardDefault();
        if (!m_artboard)
        {
            std::cerr << "No artboard found" << std::endl;
            return false;
        }

        m_scene = m_artboard->defaultStateMachine();
        if (!m_scene)
            m_scene = m_artboard->defaultScene();
        if (!m_scene)
        {
            std::cerr << "No scene or state machine found" << std::endl;
            return false;
        }

        std::cout << "Loaded: artboard=\"" << m_artboard->name()
                  << "\" scene=\"" << m_scene->name() << "\"" << std::endl;

        return true;
    }

    void findJoystick()
    {
        // Advance once to initialize nested artboards
        m_scene->advanceAndApply(0.0f);

        // The joystick lives in the nested "Rive Avatar Rubick" artboard
        auto nested = m_artboard->nestedArtboards();
        std::cout << "Searching " << nested.size()
                  << " nested artboards for Joystick..." << std::endl;

        for (auto* na : nested)
        {
            auto* inst = na->artboardInstance();
            if (!inst)
                continue;

            std::cout << "  Nested: \"" << inst->name() << "\"" << std::endl;

            auto joysticks = inst->find<Joystick>();
            std::cout << "  Joysticks found: " << joysticks.size() << std::endl;

            if (!joysticks.empty())
            {
                m_joystick = joysticks[0];
                m_nestedArtboard = inst;
                std::cout << "  Joystick bound! x=" << m_joystick->x()
                          << " y=" << m_joystick->y()
                          << " w=" << m_joystick->width()
                          << " h=" << m_joystick->height() << std::endl;
                return;
            }
        }

        // Fallback: check the main artboard directly
        auto joysticks = m_artboard->find<Joystick>();
        if (!joysticks.empty())
        {
            m_joystick = joysticks[0];
            m_nestedArtboard = m_artboard.get();
            std::cout << "Joystick found in main artboard!" << std::endl;
            return;
        }

        std::cerr << "WARNING: No joystick found!" << std::endl;
    }

    bool connectSceneSocket(const char* socketPath)
    {
        m_sceneSocket = socket(AF_UNIX, SOCK_DGRAM, 0);
        if (m_sceneSocket < 0)
        {
            perror("socket");
            return false;
        }

        int flags = fcntl(m_sceneSocket, F_GETFL, 0);
        fcntl(m_sceneSocket, F_SETFL, flags | O_NONBLOCK);

        unlink(socketPath);

        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, socketPath, sizeof(addr.sun_path) - 1);

        if (bind(m_sceneSocket, (struct sockaddr*)&addr, sizeof(addr)) < 0)
        {
            perror("bind");
            close(m_sceneSocket);
            m_sceneSocket = -1;
            return false;
        }

        chmod(socketPath, 0666);
        std::cout << "Bound to scene socket: " << socketPath << std::endl;
        return true;
    }

    void run()
    {
        std::cout << "\n=== Starting Demo ===" << std::endl;
        std::cout << "Press Ctrl+C to quit" << std::endl;

        auto lastTime = std::chrono::high_resolution_clock::now();

        while (g_running)
        {
            auto currentTime = std::chrono::high_resolution_clock::now();
            std::chrono::duration<float> elapsed = currentTime - lastTime;
            float dt = elapsed.count();
            lastTime = currentTime;

            pollDetections();
            updateEyeGaze(dt);
            applyJoystick();

            if (m_scene)
                m_scene->advanceAndApply(dt);

            renderFrame();
        }

        std::cout << "\n=== Shutting Down ===" << std::endl;
    }

private:
    void pollDetections()
    {
        if (m_sceneSocket < 0)
            return;

        char buffer[65536];
        bool gotAny = false;
        for (;;)
        {
            ssize_t len = recv(m_sceneSocket, buffer, sizeof(buffer) - 1, 0);
            if (len <= 0)
                break;
            buffer[len] = '\0';

            std::string json(buffer, len);
            DetectionData data;
            if (parseDetectionJSON(json, data))
            {
                m_latestDetections = data;
                gotAny = true;
            }
        }
        if (gotAny)
            m_hasNewDetection = true;
    }

    void updateEyeGaze(float dt)
    {
        if (!m_joystick)
            return;

        float targetX = 0.0f;
        float targetY = 0.0f;

        if (m_hasNewDetection)
        {
            m_hasNewDetection = false;

            Detection* person = nullptr;
            float bestConf = 0;
            for (auto& det : m_latestDetections.objects)
            {
                if (det.label == "person" && det.conf > 0.3f &&
                    det.conf > bestConf)
                {
                    person = &det;
                    bestConf = det.conf;
                }
            }

            if (person)
            {
                float centerX =
                    (person->box.left + person->box.right) / 2.0f;
                float centerY =
                    (person->box.top + person->box.bottom) / 2.0f;

                // Camera AI frame is 640x640.
                // Map to [-1, 1]: left=-1, right=+1, top=-1, bottom=+1
                m_targetGazeX = (centerX / 640.0f) * 2.0f - 1.0f;
                m_targetGazeY = (centerY / 640.0f) * 2.0f - 1.0f;

                // Clamp
                m_targetGazeX =
                    std::max(-1.0f, std::min(1.0f, m_targetGazeX));
                m_targetGazeY =
                    std::max(-1.0f, std::min(1.0f, m_targetGazeY));

                m_personVisible = true;
                m_timeSinceLastPerson = 0.0f;
            }
            else
            {
                m_timeSinceLastPerson += dt;
                // Return to center after 1 second of no person
                if (m_timeSinceLastPerson > 1.0f)
                {
                    m_targetGazeX = 0.0f;
                    m_targetGazeY = 0.0f;
                    m_personVisible = false;
                }
            }
        }
        else
        {
            m_timeSinceLastPerson += dt;
            if (m_timeSinceLastPerson > 1.0f)
            {
                m_targetGazeX = 0.0f;
                m_targetGazeY = 0.0f;
                m_personVisible = false;
            }
        }

        // Smooth interpolation (exponential lerp)
        // Faster when tracking person, slower when returning to center
        float speed = m_personVisible ? 8.0f : 3.0f;
        float t = 1.0f - std::exp(-speed * dt);
        m_currentGazeX += (m_targetGazeX - m_currentGazeX) * t;
        m_currentGazeY += (m_targetGazeY - m_currentGazeY) * t;

        // Log gaze changes periodically
        static int frameCount = 0;
        if (++frameCount % 60 == 0)
        {
            std::cout << "gaze: x=" << std::fixed << std::setprecision(2)
                      << m_currentGazeX << " y=" << m_currentGazeY
                      << (m_personVisible ? " [tracking]" : " [idle]")
                      << std::endl
                      << std::flush;
        }
    }

    void applyJoystick()
    {
        if (!m_joystick || !m_nestedArtboard)
            return;

        m_joystick->x(m_currentGazeX);
        m_joystick->y(m_currentGazeY);
        m_joystick->apply(m_nestedArtboard);
    }

    void renderFrame()
    {
        static int frameCount = 0;
        static auto lastFpsReport = std::chrono::steady_clock::now();
        frameCount++;
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           now - lastFpsReport)
                           .count();
        if (elapsed >= 5000)
        {
            double fps = (frameCount * 1000.0) / elapsed;
            std::cout << "FPS: " << std::fixed << std::setprecision(1) << fps;
            if (m_showTiming)
            {
                std::cout << "  (advance=" << std::setprecision(1)
                          << m_avgAdvanceMs << "ms"
                          << " draw=" << m_avgDrawMs << "ms"
                          << " flush=" << m_avgFlushMs << "ms"
                          << " blit=" << m_avgBlitMs << "ms"
                          << " swap=" << m_avgSwapMs << "ms)";
            }
            std::cout << std::endl;
            frameCount = 0;
            lastFpsReport = now;
        }

        m_drmContext->beginFrame();

        if (!m_artboard || !m_renderer)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, m_width, m_height);
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            m_drmContext->swapBuffers();
            return;
        }

        auto t0 = std::chrono::high_resolution_clock::now();

        RenderContext::FrameDescriptor frameDesc = {
            .renderTargetWidth = m_renderWidth,
            .renderTargetHeight = m_renderHeight,
            .clearColor = 0xff000000,
        };

        auto renderContextGL =
            m_renderContext->static_impl_cast<RenderContextGLImpl>();
        renderContextGL->invalidateGLState();
        m_renderContext->beginFrame(frameDesc);

        auto t1 = std::chrono::high_resolution_clock::now();

        float abW = m_artboard->width();
        float abH = m_artboard->height();
        if (abW <= 0 || abH <= 0)
        {
            abW = m_renderWidth;
            abH = m_renderHeight;
        }

        AABB displayBounds(0, 0, m_renderWidth, m_renderHeight);
        AABB artboardBounds(0, 0, abW, abH);

        m_renderer->save();
        m_renderer->align(
            Fit::contain, Alignment::center, displayBounds, artboardBounds);
        m_artboard->draw(m_renderer.get());
        m_renderer->restore();

        auto t2 = std::chrono::high_resolution_clock::now();

        m_renderContext->flush({.renderTarget = m_renderTarget.get()});

        auto t3 = std::chrono::high_resolution_clock::now();

        int dstX = (m_width - m_renderWidth) / 2;
        int dstY = (m_height - m_renderHeight) / 2;

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

        // Only clear the full screen for the first few frames to initialize
        // all page-flip buffers to black. After that, the 500x500 blit
        // region is the only area that changes.
        if (m_initClearFrames > 0)
        {
            glViewport(0, 0, m_width, m_height);
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            m_initClearFrames--;
        }

        glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo);
        glBlitFramebuffer(0, 0, m_renderWidth, m_renderHeight,
                          dstX, dstY, dstX + m_renderWidth,
                          dstY + m_renderHeight,
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        auto t4 = std::chrono::high_resolution_clock::now();

        m_drmContext->swapBuffers();

        auto t5 = std::chrono::high_resolution_clock::now();

        if (m_showTiming)
        {
            auto ms = [](auto a, auto b) {
                return std::chrono::duration<float, std::milli>(b - a).count();
            };
            float alpha = 0.05f;
            m_avgAdvanceMs += (ms(t0, t1) - m_avgAdvanceMs) * alpha;
            m_avgDrawMs += (ms(t1, t2) - m_avgDrawMs) * alpha;
            m_avgFlushMs += (ms(t2, t3) - m_avgFlushMs) * alpha;
            m_avgBlitMs += (ms(t3, t4) - m_avgBlitMs) * alpha;
            m_avgSwapMs += (ms(t4, t5) - m_avgSwapMs) * alpha;
        }
    }

private:
    // DRM/EGL
    std::unique_ptr<rive_rk3566::DRMEGLContext> m_drmContext;
    std::unique_ptr<RenderContext> m_renderContext;
    std::unique_ptr<RiveRenderer> m_renderer;
    rcp<RenderTarget> m_renderTarget;
    uint32_t m_width = 0, m_height = 0;
    uint32_t m_renderWidth = 500, m_renderHeight = 500;
    GLuint m_fbo = 0, m_fboTex = 0;
    int m_initClearFrames = 6;

    // Frame timing (exponential moving average)
    bool m_showTiming = false;
    float m_avgAdvanceMs = 0, m_avgDrawMs = 0, m_avgFlushMs = 0;
    float m_avgBlitMs = 0, m_avgSwapMs = 0;

    // Rive
    rcp<File> m_file;
    std::unique_ptr<ArtboardInstance> m_artboard;
    std::unique_ptr<Scene> m_scene;

    // Joystick eye control
    Joystick* m_joystick = nullptr;
    Artboard* m_nestedArtboard = nullptr;

    // Gaze state (smooth interpolation)
    float m_targetGazeX = 0.0f, m_targetGazeY = 0.0f;
    float m_currentGazeX = 0.0f, m_currentGazeY = 0.0f;
    bool m_personVisible = false;
    float m_timeSinceLastPerson = 10.0f;

    // Socket
    int m_sceneSocket = -1;
    DetectionData m_latestDetections;
    bool m_hasNewDetection = false;
};

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    bool inspectOnly = false;
    bool showTiming = false;
    const char* rivPath = nullptr;
    const char* socketPath = "/tmp/soulcam_scene.sock";
    uint32_t renderW = 500, renderH = 500;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--inspect") == 0)
            inspectOnly = true;
        else if (strcmp(argv[i], "--timing") == 0)
            showTiming = true;
        else if (strcmp(argv[i], "--socket") == 0 && i + 1 < argc)
            socketPath = argv[++i];
        else if (strcmp(argv[i], "--resolution") == 0 && i + 1 < argc)
        {
            i++;
            if (sscanf(argv[i], "%ux%u", &renderW, &renderH) != 2)
            {
                renderW = renderH = std::atoi(argv[i]);
                if (renderW == 0) renderW = renderH = 500;
            }
        }
        else if (!rivPath)
            rivPath = argv[i];
    }

    if (!rivPath)
    {
        std::cerr << "Usage: " << argv[0]
                  << " [--inspect] [--timing] [--resolution WxH] "
                     "[--socket PATH] <riv-file>\n"
                  << "  --inspect      Dump all artboard info and exit\n"
                  << "  --timing       Show per-phase frame timing\n"
                  << "  --resolution   Render resolution (default: 500x500)\n"
                  << "  --socket       Scene socket path "
                     "(default: /tmp/soulcam_scene.sock)\n"
                  << "\nSet RIVE_VSYNC=0 to disable vsync for benchmarking.\n";
        return 1;
    }

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    AvatarTrackerDemo demo;
    demo.setRenderSize(renderW, renderH);
    demo.setShowTiming(showTiming);
    if (!demo.initialize(rivPath, socketPath, inspectOnly))
    {
        if (inspectOnly)
            return 0;
        std::cerr << "Failed to initialize demo" << std::endl;
        return 1;
    }

    demo.run();
    return 0;
}
