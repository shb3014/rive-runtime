/*
 * Universal Tracker Demo
 * Automatically detects the proper control method for any .riv file and
 * maps SoulCam human detections to the matching Rive input.
 *
 * Auto-detection priority:
 *   1. Joystick (x/y continuous, e.g. avatar.riv)
 *   2. look_dir SMINumber (discrete directions, e.g. dress-up.riv)
 *   3. pointerMove() fallback (e.g. face-tracking-test.riv, anime-girl.riv)
 *
 * Works with SoulCam detection via /tmp/soulcam_scene.sock.
 */

#include "drm_egl_context.h"
#include "rive/artboard.hpp"
#include "rive/file.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/animation/state_machine_input_instance.hpp"
#include "rive/animation/nested_input.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include "rive/renderer/gl/render_context_gl_impl.hpp"
#include "rive/renderer/gl/render_target_gl.hpp"
#include "rive/math/aabb.hpp"
#include "rive/math/vec2d.hpp"
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
// Control mode enum
// ---------------------------------------------------------------------------
enum class ControlMode
{
    NONE,
    JOYSTICK,
    LOOK_DIR,
    POINTER_MOVE,
};

static const char* controlModeName(ControlMode m)
{
    switch (m)
    {
        case ControlMode::JOYSTICK:
            return "Joystick (continuous x/y)";
        case ControlMode::LOOK_DIR:
            return "look_dir (discrete directions via SMINumber)";
        case ControlMode::POINTER_MOVE:
            return "pointerMove (artboard coordinates)";
        case ControlMode::NONE:
            return "NONE — tracking disabled";
    }
    return "unknown";
}

// ---------------------------------------------------------------------------
// Main demo class
// ---------------------------------------------------------------------------
class UniversalTrackerDemo
{
public:
    UniversalTrackerDemo() = default;
    ~UniversalTrackerDemo()
    {
        if (m_sceneSocket >= 0)
            close(m_sceneSocket);
    }

    void setRenderSize(uint32_t w, uint32_t h)
    {
        m_renderWidth = w;
        m_renderHeight = h;
    }
    void setShowTiming(bool v) { m_showTiming = v; }

    bool initialize(const char* rivPath, const char* socketPath,
                    bool inspectOnly)
    {
        std::cout << "=== Universal Tracker Demo ===" << std::endl;

        std::cout << "\nInitializing DRM/EGL..." << std::endl;
        m_drmContext = std::make_unique<rive_rk3566::DRMEGLContext>();
        if (!m_drmContext->initialize())
        {
            std::cerr << "Failed to initialize DRM/EGL: "
                      << m_drmContext->lastError() << std::endl;
            return false;
        }

        m_displayWidth = m_drmContext->width();
        m_displayHeight = m_drmContext->height();

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

        detectControlMode();

        std::cout << "Setting up scene socket: " << socketPath << std::endl;
        if (!connectSceneSocket(socketPath))
        {
            std::cerr << "Warning: Failed to set up scene socket.\n"
                      << "  Run SoulCam with --ai flag, or use "
                         "test_eye_tracking.py\n";
        }

        std::cout << "\n=== Initialization Complete ===" << std::endl;
        std::cout << "Display: " << m_displayWidth << "x" << m_displayHeight
                  << std::endl;
        std::cout << "Render:  " << m_renderWidth << "x" << m_renderHeight
                  << std::endl;
        std::cout << "Artboard: " << m_artboard->name()
                  << " (" << m_artboard->width() << "x"
                  << m_artboard->height() << ")" << std::endl;
        std::cout << "Control:  " << controlModeName(m_controlMode)
                  << std::endl;

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

            switch (m_controlMode)
            {
                case ControlMode::JOYSTICK:
                    updateJoystick(dt);
                    applyJoystick();
                    break;
                case ControlMode::LOOK_DIR:
                    updateLookDir();
                    break;
                case ControlMode::POINTER_MOVE:
                    updatePointerMove(dt);
                    break;
                case ControlMode::NONE:
                    break;
            }

            if (m_charScene)
                m_charScene->advanceAndApply(dt);
            if (m_scene)
                m_scene->advanceAndApply(dt);

            renderFrame();
        }

        std::cout << "\n=== Shutting Down ===" << std::endl;
    }

private:
    // -----------------------------------------------------------------------
    // File loading
    // -----------------------------------------------------------------------
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

    // -----------------------------------------------------------------------
    // Auto-detection of control mode
    // -----------------------------------------------------------------------
    void detectControlMode()
    {
        m_scene->advanceAndApply(0.0f);

        std::cout << "\n--- Auto-detecting control mode ---" << std::endl;

        if (tryFindJoystick())
        {
            m_controlMode = ControlMode::JOYSTICK;
            std::cout << "=> Detected: JOYSTICK" << std::endl;
            return;
        }

        if (tryFindLookDir())
        {
            m_controlMode = ControlMode::LOOK_DIR;
            std::cout << "=> Detected: LOOK_DIR" << std::endl;
            return;
        }

        // pointerMove works on any state machine that has pointer listeners.
        // Also bind any trigger/bool inputs for direct driving.
        bindDirectInputs();
        m_abWidth = m_artboard->width();
        m_abHeight = m_artboard->height();
        m_currentGazeX = m_abWidth / 2.0f;
        m_currentGazeY = m_abHeight / 2.0f;
        m_targetGazeX = m_currentGazeX;
        m_targetGazeY = m_currentGazeY;

        m_controlMode = ControlMode::POINTER_MOVE;
        std::cout << "=> Fallback: POINTER_MOVE (artboard "
                  << m_abWidth << "x" << m_abHeight << ")" << std::endl;
    }

    bool tryFindJoystick()
    {
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
            if (!joysticks.empty())
            {
                m_joystick = joysticks[0];
                m_nestedArtboard = inst;
                std::cout << "  Joystick found! x=" << m_joystick->x()
                          << " y=" << m_joystick->y()
                          << " w=" << m_joystick->width()
                          << " h=" << m_joystick->height() << std::endl;
                return true;
            }
        }

        auto joysticks = m_artboard->find<Joystick>();
        if (!joysticks.empty())
        {
            m_joystick = joysticks[0];
            m_nestedArtboard = m_artboard.get();
            std::cout << "  Joystick found in main artboard!" << std::endl;
            return true;
        }

        std::cout << "  No Joystick found." << std::endl;
        return false;
    }

    bool tryFindLookDir()
    {
        auto nested = m_artboard->nestedArtboards();
        std::cout << "Searching nested artboards for look_dir input..."
                  << std::endl;

        for (auto* na : nested)
        {
            auto* inst = na->artboardInstance();
            if (!inst)
                continue;

            // Try NestedInput API first
            auto* nestedInput = na->input("look_dir");
            if (nestedInput)
            {
                auto* smiInput = nestedInput->input();
                if (smiInput)
                {
                    m_lookDirInput = static_cast<SMINumber*>(smiInput);
                    std::cout << "  look_dir found via NestedInput in \""
                              << inst->name() << "\"" << std::endl;
                    return true;
                }
            }

            // Fallback: direct SM instantiation
            for (size_t s = 0; s < inst->stateMachineCount(); s++)
            {
                auto sm = inst->stateMachineAt(s);
                if (!sm)
                    continue;
                for (size_t i = 0; i < sm->inputCount(); i++)
                {
                    auto* inp = sm->input(i);
                    if (inp && inp->name() == "look_dir")
                    {
                        m_lookDirInput = static_cast<SMINumber*>(inp);
                        m_charScene = std::move(sm);
                        std::cout << "  look_dir found via direct SM in \""
                                  << inst->name() << "\"" << std::endl;
                        return true;
                    }
                }
            }
        }

        // Also check the main scene's own inputs
        for (size_t i = 0; i < m_scene->inputCount(); i++)
        {
            auto* inp = m_scene->input(i);
            if (inp && inp->name() == "look_dir")
            {
                m_lookDirInput = static_cast<SMINumber*>(inp);
                std::cout << "  look_dir found in main scene!" << std::endl;
                return true;
            }
        }

        std::cout << "  No look_dir input found." << std::endl;
        return false;
    }

    void bindDirectInputs()
    {
        std::cout << "Binding state machine inputs for direct control..."
                  << std::endl;

        if (m_scene->inputCount() == 0)
        {
            std::cout << "  No inputs, skip." << std::endl;
            return;
        }

        for (size_t i = 0; i < m_scene->inputCount(); i++)
        {
            auto* inp = m_scene->input(i);
            if (!inp)
                continue;
            const auto& name = inp->name();
            uint16_t coreType = inp->inputCoreType();
            std::cout << "  Input[" << i << "] \"" << name
                      << "\" coreType=" << coreType << std::endl;

            // Bool inputs: enable tracking-related ones on startup
            // coreType 59 = StateMachineBool
            if (coreType == 59)
            {
                bool enable =
                    (name == "Parent-isTracking" || name == "isTracking" ||
                     name == "tracking" || name == "isOnGlasses");
                if (enable)
                {
                    static_cast<SMIBool*>(inp)->value(true);
                    std::cout << "    -> enabled (set true)" << std::endl;
                }
                // Presence-reactive bools (toggle based on person visibility)
                if (name.find("blush") != std::string::npos ||
                    name.find("Blush") != std::string::npos ||
                    name.find("blushing") != std::string::npos)
                {
                    m_presenceBoolInputs.push_back(
                        static_cast<SMIBool*>(inp));
                    std::cout << "    -> bound as presence-reactive bool"
                              << std::endl;
                }
            }

            // Trigger inputs: collect for position-based firing
            // coreType 58 = StateMachineTrigger
            if (coreType == 58)
            {
                bool isLeft =
                    (name.find("left") != std::string::npos ||
                     name.find("Left") != std::string::npos);
                bool isRight =
                    (name.find("right") != std::string::npos ||
                     name.find("Right") != std::string::npos);

                if (isLeft)
                {
                    m_triggerLeft = static_cast<SMITrigger*>(inp);
                    std::cout << "    -> bound as LEFT trigger" << std::endl;
                }
                else if (isRight)
                {
                    m_triggerRight = static_cast<SMITrigger*>(inp);
                    std::cout << "    -> bound as RIGHT trigger" << std::endl;
                }
                else
                {
                    m_triggerCenter.push_back(
                        static_cast<SMITrigger*>(inp));
                    std::cout << "    -> bound as CENTER trigger" << std::endl;
                }
            }
        }

        bool hasTriggers = m_triggerLeft || m_triggerRight ||
                           !m_triggerCenter.empty();
        bool hasBools = !m_presenceBoolInputs.empty();
        if (hasTriggers || hasBools)
        {
            std::cout << "  Direct input bindings: "
                      << (m_triggerLeft ? "LEFT " : "")
                      << (m_triggerRight ? "RIGHT " : "")
                      << m_triggerCenter.size() << " center trigger(s), "
                      << m_presenceBoolInputs.size()
                      << " presence bool(s)" << std::endl;
        }
    }

    // -----------------------------------------------------------------------
    // Socket
    // -----------------------------------------------------------------------
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

    // -----------------------------------------------------------------------
    // Detection polling
    // -----------------------------------------------------------------------
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

    Detection* findBestPerson()
    {
        Detection* best = nullptr;
        float bestConf = 0;
        for (auto& det : m_latestDetections.objects)
        {
            if (det.label == "person" && det.conf > 0.3f &&
                det.conf > bestConf)
            {
                best = &det;
                bestConf = det.conf;
            }
        }
        return best;
    }

    // -----------------------------------------------------------------------
    // JOYSTICK control
    // -----------------------------------------------------------------------
    void updateJoystick(float dt)
    {
        if (!m_joystick)
            return;

        if (m_hasNewDetection)
        {
            m_hasNewDetection = false;
            Detection* person = findBestPerson();

            if (person)
            {
                float centerX =
                    (person->box.left + person->box.right) / 2.0f;
                float centerY =
                    (person->box.top + person->box.bottom) / 2.0f;

                m_targetGazeX =
                    std::clamp((centerX / 640.0f) * 2.0f - 1.0f, -1.0f, 1.0f);
                m_targetGazeY =
                    std::clamp((centerY / 640.0f) * 2.0f - 1.0f, -1.0f, 1.0f);
                m_personVisible = true;
                m_timeSinceLastPerson = 0.0f;
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

        float speed = m_personVisible ? 8.0f : 3.0f;
        float t = 1.0f - std::exp(-speed * dt);
        m_currentGazeX += (m_targetGazeX - m_currentGazeX) * t;
        m_currentGazeY += (m_targetGazeY - m_currentGazeY) * t;

        logGaze(60);
    }

    void applyJoystick()
    {
        if (!m_joystick || !m_nestedArtboard)
            return;
        m_joystick->x(m_currentGazeX);
        m_joystick->y(m_currentGazeY);
        m_joystick->apply(m_nestedArtboard);
    }

    // -----------------------------------------------------------------------
    // LOOK_DIR control
    // -----------------------------------------------------------------------
    void updateLookDir()
    {
        if (!m_lookDirInput || !m_hasNewDetection)
            return;
        m_hasNewDetection = false;

        Detection* person = findBestPerson();
        float newDir = 0.0f;

        if (person)
        {
            float normX =
                ((person->box.left + person->box.right) / 2.0f) / 640.0f;

            if (normX < 0.33f)
                newDir = 1.0f;
            else if (normX > 0.66f)
                newDir = 2.0f;
            else
                newDir = 3.0f;
        }

        if (newDir != m_currentLookDir)
        {
            m_currentLookDir = newDir;
            m_lookDirInput->value(newDir);

            static const char* dirNames[] = {"center", "dir_1", "dir_2",
                                             "dir_3"};
            int dirIdx = static_cast<int>(newDir);
            std::cout << "look_dir -> " << newDir << " ("
                      << (dirIdx < 4 ? dirNames[dirIdx] : "?") << ")"
                      << std::endl;
        }
    }

    // -----------------------------------------------------------------------
    // POINTER_MOVE control
    //
    // Sends pointerMove for hover-based tracking, plus pointerDown/pointerUp
    // for .riv files that use click/hit-test listeners (triggers).
    //
    // On person appear  → pointerDown (simulate touch start)
    // On person present → pointerMove (continuous tracking)
    // On person vanish  → pointerUp   (simulate touch release)
    // Periodic re-click every ~2 seconds while tracking to re-trigger
    // hit-test listeners in case the pointer moved to a new hit area.
    // -----------------------------------------------------------------------
    void updatePointerMove(float dt)
    {
        bool wasVisible = m_personVisible;

        if (m_hasNewDetection)
        {
            m_hasNewDetection = false;
            Detection* person = findBestPerson();

            if (person)
            {
                float centerX =
                    (person->box.left + person->box.right) / 2.0f;
                float centerY =
                    (person->box.top + person->box.bottom) / 2.0f;

                m_targetGazeX = (centerX / 640.0f) * m_abWidth;
                m_targetGazeY = (centerY / 640.0f) * m_abHeight;
                m_personVisible = true;
                m_timeSinceLastPerson = 0.0f;
            }
            else
            {
                m_timeSinceLastPerson += dt;
                if (m_timeSinceLastPerson > 1.0f)
                {
                    m_targetGazeX = m_abWidth / 2.0f;
                    m_targetGazeY = m_abHeight / 2.0f;
                    m_personVisible = false;
                }
            }
        }
        else
        {
            m_timeSinceLastPerson += dt;
            if (m_timeSinceLastPerson > 1.0f)
            {
                m_targetGazeX = m_abWidth / 2.0f;
                m_targetGazeY = m_abHeight / 2.0f;
                m_personVisible = false;
            }
        }

        float speed = m_personVisible ? 8.0f : 3.0f;
        float t = 1.0f - std::exp(-speed * dt);
        m_currentGazeX += (m_targetGazeX - m_currentGazeX) * t;
        m_currentGazeY += (m_targetGazeY - m_currentGazeY) * t;

        Vec2D pos(m_currentGazeX, m_currentGazeY);
        m_scene->pointerMove(pos);

        // Person just appeared → press down
        if (m_personVisible && !wasVisible)
        {
            m_scene->pointerDown(pos);
            m_pointerIsDown = true;
            m_clickTimer = 0.0f;
            std::cout << "pointerDown at (" << std::fixed
                      << std::setprecision(0) << pos.x << ", " << pos.y << ")"
                      << std::endl;
        }

        // Person just vanished → release
        if (!m_personVisible && wasVisible && m_pointerIsDown)
        {
            m_scene->pointerUp(pos);
            m_pointerIsDown = false;
            std::cout << "pointerUp at (" << std::fixed
                      << std::setprecision(0) << pos.x << ", " << pos.y << ")"
                      << std::endl;
        }

        // Re-click periodically while tracking to re-trigger hit-test areas
        // as the pointer sweeps across the artboard.
        if (m_personVisible && m_pointerIsDown)
        {
            m_clickTimer += dt;
            if (m_clickTimer >= 2.0f)
            {
                m_scene->pointerUp(pos);
                m_scene->pointerDown(pos);
                m_clickTimer = 0.0f;
            }
        }

        // Drive presence-reactive bools (e.g. "is blushing" → true while
        // person visible)
        for (auto* b : m_presenceBoolInputs)
            b->value(m_personVisible);

        // Drive positional triggers: fire left/right/center based on where
        // the person is in the frame. Triggers auto-reset after one advance
        // cycle, so we re-fire on zone transitions.
        if (m_personVisible && (m_triggerLeft || m_triggerRight))
        {
            float normX = m_currentGazeX / m_abWidth;
            int zone = (normX < 0.4f) ? -1 : (normX > 0.6f) ? 1 : 0;

            if (zone != m_lastTriggerZone)
            {
                m_lastTriggerZone = zone;
                if (zone < 0 && m_triggerLeft)
                {
                    m_triggerLeft->fire();
                    std::cout << "trigger: LEFT" << std::endl;
                }
                else if (zone > 0 && m_triggerRight)
                {
                    m_triggerRight->fire();
                    std::cout << "trigger: RIGHT" << std::endl;
                }
                else
                {
                    for (auto* t : m_triggerCenter)
                        t->fire();
                    if (!m_triggerCenter.empty())
                        std::cout << "trigger: CENTER" << std::endl;
                }
            }
        }
        else if (!m_personVisible)
        {
            m_lastTriggerZone = 999;
        }

        logGaze(60);
    }

    // -----------------------------------------------------------------------
    // Logging
    // -----------------------------------------------------------------------
    void logGaze(int interval)
    {
        if (++m_gazeLogCounter % interval == 0)
        {
            if (m_controlMode == ControlMode::POINTER_MOVE)
            {
                std::cout << "gaze: (" << std::fixed << std::setprecision(0)
                          << m_currentGazeX << ", " << m_currentGazeY << ")";
            }
            else
            {
                std::cout << "gaze: x=" << std::fixed << std::setprecision(2)
                          << m_currentGazeX << " y=" << m_currentGazeY;
            }
            std::cout << (m_personVisible ? " [tracking]" : " [idle]")
                      << std::endl;
        }
    }

    // -----------------------------------------------------------------------
    // Rendering
    // -----------------------------------------------------------------------
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
            glViewport(0, 0, m_displayWidth, m_displayHeight);
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

        int dstX = (m_displayWidth - m_renderWidth) / 2;
        int dstY = (m_displayHeight - m_renderHeight) / 2;

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

        if (m_initClearFrames > 0)
        {
            glViewport(0, 0, m_displayWidth, m_displayHeight);
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

    // -----------------------------------------------------------------------
    // Member data
    // -----------------------------------------------------------------------

    // DRM/EGL
    std::unique_ptr<rive_rk3566::DRMEGLContext> m_drmContext;
    std::unique_ptr<RenderContext> m_renderContext;
    std::unique_ptr<RiveRenderer> m_renderer;
    rcp<RenderTarget> m_renderTarget;
    uint32_t m_displayWidth = 0, m_displayHeight = 0;
    uint32_t m_renderWidth = 500, m_renderHeight = 500;
    GLuint m_fbo = 0, m_fboTex = 0;
    int m_initClearFrames = 6;

    bool m_showTiming = false;
    float m_avgAdvanceMs = 0, m_avgDrawMs = 0, m_avgFlushMs = 0;
    float m_avgBlitMs = 0, m_avgSwapMs = 0;

    // Rive
    rcp<File> m_file;
    std::unique_ptr<ArtboardInstance> m_artboard;
    std::unique_ptr<Scene> m_scene;
    std::unique_ptr<StateMachineInstance> m_charScene;

    // Control mode
    ControlMode m_controlMode = ControlMode::NONE;

    // Joystick mode
    Joystick* m_joystick = nullptr;
    Artboard* m_nestedArtboard = nullptr;

    // look_dir mode
    SMINumber* m_lookDirInput = nullptr;
    float m_currentLookDir = 0.0f;

    // Shared gaze state (Joystick and pointerMove)
    float m_targetGazeX = 0.0f, m_targetGazeY = 0.0f;
    float m_currentGazeX = 0.0f, m_currentGazeY = 0.0f;
    float m_abWidth = 500.0f, m_abHeight = 500.0f;
    bool m_personVisible = false;
    float m_timeSinceLastPerson = 10.0f;
    int m_gazeLogCounter = 0;

    // pointerMove mode: click simulation state
    bool m_pointerIsDown = false;
    float m_clickTimer = 0.0f;

    // Direct SMI bindings for trigger/bool inputs
    SMITrigger* m_triggerLeft = nullptr;
    SMITrigger* m_triggerRight = nullptr;
    std::vector<SMITrigger*> m_triggerCenter;
    std::vector<SMIBool*> m_presenceBoolInputs;
    int m_lastTriggerZone = 999;

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
                if (renderW == 0)
                    renderW = renderH = 500;
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
                  << "\nAuto-detects control mode (Joystick / look_dir / "
                     "pointerMove)\n"
                  << "and maps SoulCam human detection to matching Rive "
                     "input.\n\n"
                  << "Options:\n"
                  << "  --inspect      Dump all artboard info and exit\n"
                  << "  --timing       Show per-phase frame timing\n"
                  << "  --resolution   Render resolution (default: 500x500)\n"
                  << "  --socket       Scene socket path "
                     "(default: /tmp/soulcam_scene.sock)\n"
                  << "\nSet RIVE_VSYNC=0 to disable vsync.\n";
        return 1;
    }

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    UniversalTrackerDemo demo;
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
