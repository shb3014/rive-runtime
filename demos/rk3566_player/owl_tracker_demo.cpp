/*
 * Owl Eye Tracker Demo
 * Combines SoulCam human detection with Rive animation.
 * The owl's eyes track detected humans in the camera frame.
 *
 * Architecture:
 *   dress-up.riv has 3 artboards: "scene", "character", "bubble"
 *   "scene" is the top-level; it nests "character" which has a state machine
 *   "pose_statement" with inputs: fx, equip_change, look_dir.
 *   "look_dir" (number) controls which look animation plays:
 *     look_C = center, look_1/2/3 = directional.
 *
 *   We bind to /tmp/soulcam_scene.sock to receive SoulCam detection JSON
 *   directly (the socket SoulCam sendto()'s), then map the detected person's
 *   horizontal position to look_dir values.
 */

#include "drm_egl_context.h"
#include "rive/artboard.hpp"
#include "rive/file.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/animation/nested_input.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include "rive/renderer/gl/render_context_gl_impl.hpp"
#include "rive/renderer/gl/render_target_gl.hpp"
#include "rive/math/aabb.hpp"
#include "rive/node.hpp"
#include "rive/bones/bone.hpp"
#include "rive/joystick.hpp"
#include "rive/nested_artboard.hpp"
#include "rive/shapes/shape.hpp"
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
#include <poll.h>
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

    // Handle both "objects":[  and  "objects": [  (with/without space)
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

        // Helper: find a JSON key and skip past ": " or ":" to the value
        auto findValue = [&](const std::string& s, const char* key,
                             size_t from) -> size_t {
            std::string k = std::string("\"") + key + "\"";
            size_t p = s.find(k, from);
            if (p == std::string::npos)
                return std::string::npos;
            p = s.find(':', p + k.size());
            if (p == std::string::npos)
                return std::string::npos;
            // Skip optional whitespace
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

        // Box parsing
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
// Artboard inspector
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

    auto nodes = ab->find<Node>();
    std::cout << prefix << "Nodes (" << nodes.size() << "):" << std::endl;
    for (auto* n : nodes)
    {
        std::cout << prefix << "  - \"" << n->name() << "\" x=" << n->x()
                  << " y=" << n->y() << " (coreType " << n->coreType() << ")"
                  << std::endl;
    }

    auto bones = ab->find<Bone>();
    std::cout << prefix << "Bones (" << bones.size() << "):" << std::endl;
    for (auto* b : bones)
        std::cout << prefix << "  - \"" << b->name() << "\"" << std::endl;

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
        std::cout << prefix << "  - \"" << na->name() << "\" (coreType "
                  << na->coreType() << ")" << std::endl;
        auto* inst = na->artboardInstance();
        if (inst)
        {
            std::cout << prefix << "    [instance]:" << std::endl;
            inspectSingleArtboard(inst, prefix + "    ");
        }
        auto* src = na->sourceArtboard();
        if (src && src != inst)
        {
            std::cout << prefix << "    [source]: \"" << src->name() << "\" "
                      << src->width() << "x" << src->height() << std::endl;
        }
    }

    const auto& objs = ab->objects();
    std::cout << prefix << "All objects (" << objs.size() << "):" << std::endl;
    for (size_t i = 0; i < objs.size(); i++)
    {
        auto* o = objs[i];
        if (!o)
            continue;
        std::string objName;
        if (o->is<Component>())
            objName = o->as<Component>()->name();
        std::cout << prefix << "  [" << std::setw(3) << i
                  << "] coreType=" << std::setw(4) << o->coreType() << "  \""
                  << objName << "\"" << std::endl;
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
                              << " coreType=" << inp->inputCoreType()
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
class OwlTrackerDemo
{
public:
    OwlTrackerDemo() = default;
    ~OwlTrackerDemo()
    {
        if (m_sceneSocket >= 0)
            close(m_sceneSocket);
    }

    bool initialize(const char* rivPath, const char* socketPath,
                    bool inspectOnly)
    {
        std::cout << "=== Owl Eye Tracker Demo ===" << std::endl;

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

        std::cout << "Initializing Rive renderer..." << std::endl;
        RenderContextGLImpl::ContextOptions ctxOpts = {};
        m_renderContext = RenderContextGLImpl::MakeContext(ctxOpts);
        if (!m_renderContext)
        {
            std::cerr << "Failed to create Rive render context" << std::endl;
            return false;
        }

        GLint sampleCount = 0;
        glGetIntegerv(GL_SAMPLES, &sampleCount);
        m_renderTarget = make_rcp<FramebufferRenderTargetGL>(
            m_width, m_height, 0, sampleCount);

        std::cout << "Loading Rive file: " << rivPath << std::endl;
        if (!loadRiveFile(rivPath))
            return false;

        if (inspectOnly)
        {
            inspectFile(m_file.get());
            return false;
        }

        m_renderer = std::make_unique<RiveRenderer>(m_renderContext.get());

        findLookDirInput();

        std::cout << "Setting up scene socket: " << socketPath << std::endl;
        if (!connectSceneSocket(socketPath))
        {
            std::cerr << "Warning: Failed to set up scene socket." << std::endl;
            std::cerr << "Run SoulCam with --ai flag, or use "
                         "test_eye_tracking.py"
                      << std::endl;
        }

        std::cout << "\n=== Initialization Complete ===" << std::endl;
        std::cout << "Display: " << m_width << "x" << m_height << std::endl;
        std::cout << "Artboard: " << m_artboard->name()
                  << " (" << m_artboard->width() << "x"
                  << m_artboard->height() << ")" << std::endl;
        if (m_scene)
            std::cout << "Scene: " << m_scene->name()
                      << " inputs=" << m_scene->inputCount() << std::endl;
        std::cout << "Eye control: "
                  << (m_lookDirInput ? "look_dir (nested character)"
                                    : "NONE — eye tracking disabled")
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
                  << "\" scene=\"" << m_scene->name()
                  << "\" inputs=" << m_scene->inputCount() << std::endl;

        return true;
    }

    void findLookDirInput()
    {
        // The "scene" artboard nests "character" which has state machine
        // "pose_statement" with inputs: fx, equip_change, look_dir.
        // We need to advance the scene once to initialize nested artboards,
        // then access the character's state machine via direct instantiation.

        // First advance scene to initialize everything
        std::cout << "Advancing scene once to init nested artboards..."
                  << std::endl;
        m_scene->advanceAndApply(0.0f);

        auto nested = m_artboard->nestedArtboards();
        std::cout << "Searching " << nested.size()
                  << " nested artboards for look_dir..." << std::endl;

        for (size_t idx = 0; idx < nested.size(); idx++)
        {
            auto* na = nested[idx];
            auto* inst = na->artboardInstance();
            if (!inst)
            {
                std::cout << "  [" << idx << "] no instance" << std::endl;
                continue;
            }

            std::cout << "  [" << idx << "] \"" << inst->name() << "\""
                      << std::endl;

            if (inst->name() != "character")
                continue;

            // Try NestedInput API (safe after advance)
            std::cout << "  Trying NestedInput API..." << std::endl;
            auto* nestedInput = na->input("look_dir");
            if (nestedInput)
            {
                std::cout << "  Found NestedInput for look_dir" << std::endl;
                auto* smiInput = nestedInput->input();
                if (smiInput)
                {
                    // We know from inspection this is a Number input
                    m_lookDirInput = static_cast<SMINumber*>(smiInput);
                    std::cout << "  SUCCESS: look_dir bound! value="
                              << m_lookDirInput->value() << std::endl;
                    return;
                }
                else
                {
                    std::cout << "  NestedInput found but underlying SMI is "
                                 "null"
                              << std::endl;
                }
            }
            else
            {
                std::cout << "  NestedInput('look_dir') returned null"
                          << std::endl;
            }

            // Fallback: instantiate character's state machine directly
            std::cout << "  Trying direct SM instantiation..." << std::endl;
            for (size_t s = 0; s < inst->stateMachineCount(); s++)
            {
                auto sm = inst->stateMachineAt(s);
                if (!sm)
                    continue;
                std::cout << "  SM[" << s << "] \"" << sm->name()
                          << "\" inputs=" << sm->inputCount() << std::endl;
                for (size_t i = 0; i < sm->inputCount(); i++)
                {
                    auto* inp = sm->input(i);
                    if (!inp)
                        continue;
                    std::cout << "    input[" << i << "] \"" << inp->name()
                              << "\"" << std::endl;
                    if (inp->name() == "look_dir")
                    {
                        m_lookDirInput = static_cast<SMINumber*>(inp);
                        m_charScene = std::move(sm);
                        std::cout << "  SUCCESS: look_dir bound via direct SM!"
                                  << " value=" << m_lookDirInput->value()
                                  << std::endl;
                        return;
                    }
                }
            }
        }

        std::cerr << "WARNING: look_dir input not found!" << std::endl;
        std::cerr << "Run with --inspect to see file structure." << std::endl;
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
            float deltaTime = elapsed.count();
            lastTime = currentTime;

            pollDetections();
            updateLookDirection();

            if (m_charScene)
                m_charScene->advanceAndApply(deltaTime);
            if (m_scene)
                m_scene->advanceAndApply(deltaTime);

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

    void updateLookDirection()
    {
        if (!m_lookDirInput || !m_hasNewDetection)
            return;
        m_hasNewDetection = false;

        // Find best person detection
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

        // Map person position to look_dir value
        // Camera AI frame is 640x640, origin at top-left
        // look_C = center (0), look_1/2/3 = directional
        //
        // We'll map horizontal position to look direction:
        //   0 = center (no person or person in center)
        //   1 = look left  (person on left 1/3 of frame)
        //   2 = look right (person on right 1/3 of frame)
        //   3 = look up/somewhere else (person on top 1/3)
        //
        // We experiment with the mapping by trying different values.
        float newDir = 0.0f; // default center

        if (person)
        {
            float centerX = (person->box.left + person->box.right) / 2.0f;
            float centerY = (person->box.top + person->box.bottom) / 2.0f;

            // Normalize to [0, 1]
            float normX = centerX / 640.0f;

            // Map to discrete directions
            if (normX < 0.33f)
                newDir = 1.0f; // left third
            else if (normX > 0.66f)
                newDir = 2.0f; // right third
            else
                newDir = 3.0f; // center-ish → try 3 (might be "look at camera")

            (void)centerY;
        }

        // Only update if direction changed (avoid constant state changes)
        if (newDir != m_currentLookDir)
        {
            m_currentLookDir = newDir;
            m_lookDirInput->value(newDir);

            static const char* dirNames[] = {"center", "dir_1", "dir_2",
                                             "dir_3"};
            int dirIdx = static_cast<int>(newDir);
            std::cout << "look_dir -> " << newDir << " ("
                      << (dirIdx < 4 ? dirNames[dirIdx] : "?") << ")"
                      << std::endl
                      << std::flush;
        }

        static int frameCount = 0;
        if (++frameCount % 120 == 0)
        {
            if (person)
                std::cout << "Person conf=" << std::fixed
                          << std::setprecision(2) << person->conf
                          << " look_dir=" << m_currentLookDir << std::endl;
            else
                std::cout << "No person, look_dir=0 (center)" << std::endl;
        }
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
            std::cout << "FPS: " << std::fixed << std::setprecision(1) << fps
                      << std::endl;
            frameCount = 0;
            lastFpsReport = now;
        }

        m_drmContext->beginFrame();
        glViewport(0, 0, m_width, m_height);

        if (!m_artboard || !m_renderer)
        {
            m_drmContext->swapBuffers();
            return;
        }

        RenderContext::FrameDescriptor frameDesc = {
            .renderTargetWidth = m_width,
            .renderTargetHeight = m_height,
            .clearColor = 0xff000000,
        };

        auto renderContextGL =
            m_renderContext->static_impl_cast<RenderContextGLImpl>();
        renderContextGL->invalidateGLState();
        m_renderContext->beginFrame(frameDesc);

        float abW = m_artboard->width();
        float abH = m_artboard->height();
        if (abW <= 0 || abH <= 0)
        {
            abW = m_width;
            abH = m_height;
        }

        AABB displayBounds(0, 0, m_width, m_height);
        AABB artboardBounds(0, 0, abW, abH);

        m_renderer->save();
        m_renderer->align(
            Fit::contain, Alignment::center, displayBounds, artboardBounds);
        m_artboard->draw(m_renderer.get());
        m_renderer->restore();

        m_renderContext->flush({.renderTarget = m_renderTarget.get()});
        m_drmContext->swapBuffers();
    }

private:
    // DRM/EGL
    std::unique_ptr<rive_rk3566::DRMEGLContext> m_drmContext;
    std::unique_ptr<RenderContext> m_renderContext;
    std::unique_ptr<RiveRenderer> m_renderer;
    rcp<RenderTarget> m_renderTarget;
    uint32_t m_width = 0, m_height = 0;

    // Rive
    rcp<File> m_file;
    std::unique_ptr<ArtboardInstance> m_artboard;
    std::unique_ptr<Scene> m_scene;
    std::unique_ptr<StateMachineInstance> m_charScene; // character SM, if used

    // Eye tracking
    SMINumber* m_lookDirInput = nullptr;
    float m_currentLookDir = 0.0f;

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
    const char* rivPath = nullptr;
    const char* socketPath = "/tmp/soulcam_scene.sock";

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--inspect") == 0)
            inspectOnly = true;
        else if (strcmp(argv[i], "--socket") == 0 && i + 1 < argc)
            socketPath = argv[++i];
        else if (!rivPath)
            rivPath = argv[i];
    }

    if (!rivPath)
    {
        std::cerr << "Usage: " << argv[0]
                  << " [--inspect] [--socket PATH] <path-to-riv-file>\n"
                  << "  --inspect   Dump all artboard objects and exit\n"
                  << "  --socket    Scene socket path "
                     "(default: /tmp/soulcam_scene.sock)\n";
        return 1;
    }

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    OwlTrackerDemo demo;
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
