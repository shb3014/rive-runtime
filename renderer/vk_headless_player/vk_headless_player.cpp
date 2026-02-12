/*
 * Headless Vulkan FPS runner for Rive GPU renderer.
 *
 * Purpose: benchmark Vulkan backend on devices without a window system.
 *
 * Usage:
 *   PAN_I_WANT_A_BROKEN_VULKAN_DRIVER=1 \
 *   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/panfrost_icd.aarch64.json \
 *   ./out/release/vk_headless_player ~/dress-up.riv --width 500 --height 500
 */

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

#include "rive/file.hpp"
#include "rive/artboard.hpp"
#include "rive/scene.hpp"
#include "rive/animation/linear_animation_instance.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/renderer/rive_renderer.hpp"

#include "rive_vk_bootstrap/vulkan_device.hpp"
#include "rive_vk_bootstrap/vulkan_headless_frame_synchronizer.hpp"
#include "rive_vk_bootstrap/vulkan_instance.hpp"

#include "rive/renderer/vulkan/render_context_vulkan_impl.hpp"
#include "rive/renderer/vulkan/render_target_vulkan.hpp"

using namespace rive;
using namespace rive::gpu;

static bool parse_u32_arg(int& i, int argc, const char** argv, const char* name, uint32_t* out)
{
    if (strcmp(argv[i], name) != 0)
    {
        return false;
    }
    if (i + 1 >= argc)
    {
        std::cerr << "Missing value after " << name << std::endl;
        std::exit(2);
    }
    *out = static_cast<uint32_t>(std::stoul(argv[i + 1]));
    i += 1;
    return true;
}

static std::vector<uint8_t> read_file(const char* path)
{
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open())
    {
        std::cerr << "Failed to open file: " << path << std::endl;
        return {};
    }
    std::streamsize size = f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    if (!f.read(reinterpret_cast<char*>(buffer.data()), size))
    {
        std::cerr << "Failed to read file: " << path << std::endl;
        return {};
    }
    return buffer;
}

int main(int argc, const char** argv)
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);

    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0]
                  << " <path-to-riv-file> [--width N --height N] [--seconds N]\n";
        return 2;
    }

    const char* rivPath = argv[1];
    uint32_t width = 500;
    uint32_t height = 500;
    uint32_t seconds = 30;
    bool forceAtomicMode = false;

    for (int i = 2; i < argc; ++i)
    {
        if (parse_u32_arg(i, argc, argv, "--width", &width) ||
            parse_u32_arg(i, argc, argv, "--height", &height) ||
            parse_u32_arg(i, argc, argv, "--seconds", &seconds))
        {
            continue;
        }
        if (strcmp(argv[i], "--atomic") == 0)
        {
            forceAtomicMode = true;
            continue;
        }
        std::cerr << "Unknown arg: " << argv[i] << std::endl;
        return 2;
    }

    // If the user forgot these, warn (do not override).
    if (std::getenv("VK_ICD_FILENAMES") == nullptr)
    {
        std::cerr
            << "warning: VK_ICD_FILENAMES not set. On PanVK you likely want:\n"
            << "  VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/panfrost_icd.aarch64.json\n";
    }
    if (std::getenv("PAN_I_WANT_A_BROKEN_VULKAN_DRIVER") == nullptr)
    {
        std::cerr
            << "warning: PAN_I_WANT_A_BROKEN_VULKAN_DRIVER not set. On Mali-G52 (v7) PanVK requires:\n"
            << "  PAN_I_WANT_A_BROKEN_VULKAN_DRIVER=1\n";
    }

    // Setup Vulkan (headless).
    using namespace rive_vkb;
    auto instance = std::make_unique<VulkanInstance>(VulkanInstance::Options{
        .appName = "Rive Vulkan Headless Player",
        .idealAPIVersion = VK_API_VERSION_1_0,
#ifndef NDEBUG
        .desiredValidationType = VulkanValidationType::none,
        .wantDebugCallbacks = false,
#endif
    });

    auto device = std::make_unique<VulkanDevice>(*instance,
                                                 VulkanDevice::Options{
                                                     .coreFeaturesOnly = false,
                                                     .gpuNameFilter = "",
                                                     .headless = true,
                                                 });

    auto renderContext = RenderContextVulkanImpl::MakeContext(
        instance->vkInstance(),
        device->vkPhysicalDevice(),
        device->vkDevice(),
        device->vulkanFeatures(),
        instance->getVkGetInstanceProcAddrPtr(),
        {
            .forceAtomicMode = forceAtomicMode,
            .shaderCompilationMode = ShaderCompilationMode::alwaysSynchronous,
        });
    if (!renderContext)
    {
        std::cerr << "Failed to create RenderContextVulkanImpl" << std::endl;
        return 1;
    }

    // Load Rive file.
    std::vector<uint8_t> bytes = read_file(rivPath);
    if (bytes.empty())
    {
        return 1;
    }

    auto file = File::import(bytes, renderContext.get());
    if (!file)
    {
        std::cerr << "Failed to import Rive file" << std::endl;
        return 1;
    }
    auto artboard = file->artboardDefault();
    if (!artboard)
    {
        std::cerr << "No artboard found" << std::endl;
        return 1;
    }

    // Prefer state machine; fall back to first animation.
    std::unique_ptr<Scene> scene;
    if (auto sm = artboard->defaultStateMachine())
    {
        scene = std::move(sm);
    }
    else if (auto anim = artboard->animationAt(0))
    {
        scene = std::move(anim);
    }

    if (!scene)
    {
        std::cerr << "warning: no state machine or animation found; rendering static frame." << std::endl;
    }

    // Setup headless frame synchronizer + target render image.
    VkFormat imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
    VkImageUsageFlags usageFlags =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
        VK_IMAGE_USAGE_TRANSFER_DST_BIT |
        // Allow Vulkan backend to use input-attachment/storage-friendly paths.
        VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT;

    auto* impl = renderContext->static_impl_cast<RenderContextVulkanImpl>();
    auto vk = ref_rcp(impl->vulkanContext());

    auto frameSync = std::make_unique<VulkanHeadlessFrameSynchronizer>(
        *instance,
        *device,
        vk,
        VulkanHeadlessFrameSynchronizer::Options{
            .width = width,
            .height = height,
            .imageFormat = imageFormat,
            .imageUsageFlags = usageFlags,
            .initialFrameNumber = 0,
        });

    rcp<RenderTargetVulkanImpl> renderTarget = impl->makeRenderTarget(
        width,
        height,
        frameSync->imageFormat(),
        frameSync->imageUsageFlags());

    std::cout << "Vulkan device: " << device->name()
              << "\nRender size: " << width << "x" << height
              << "\nMode: " << (forceAtomicMode ? "atomic" : "default")
              << "\nSeconds: " << seconds << "\n"
              << std::endl;

    auto start = std::chrono::steady_clock::now();
    auto lastFpsReport = start;
    uint64_t frameCount = 0;
    auto lastTime = start;

    while (true)
    {
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - start).count() >=
            static_cast<long>(seconds))
        {
            break;
        }

        float dt = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;

        bool shouldDraw = true;
        if (scene)
        {
            shouldDraw = scene->advanceAndApply(dt);
        }

        if (!frameSync->isFrameStarted())
        {
            frameSync->beginFrame();
            renderTarget->setTargetImageView(frameSync->vkImageView(),
                                             frameSync->vkImage(),
                                             frameSync->lastAccess());
        }

        RenderContext::FrameDescriptor frameDesc = {
            .renderTargetWidth = width,
            .renderTargetHeight = height,
            .loadAction = LoadAction::clear,
            .clearColor = 0xff404040,
            .msaaSampleCount = 0,
            .disableRasterOrdering = false,
            .wireframe = false,
            .clockwiseFillOverride = false,
        };
        renderContext->beginFrame(frameDesc);

        if (shouldDraw)
        {
            RiveRenderer renderer(renderContext.get());
            renderer.save();
            AABB displayBounds(0, 0, width, height);
            AABB artboardBounds(0, 0, artboard->width(), artboard->height());
            renderer.align(Fit::contain, Alignment::center, displayBounds, artboardBounds);
            artboard->draw(&renderer);
            renderer.restore();
        }

        renderContext->flush({
            .renderTarget = renderTarget.get(),
            .externalCommandBuffer = frameSync->currentCommandBuffer(),
            .currentFrameNumber = frameSync->currentFrameNumber(),
            .safeFrameNumber = frameSync->safeFrameNumber(),
        });

        auto lastAccess = renderTarget->targetLastAccess();
        frameSync->endFrame(lastAccess);

        ++frameCount;

        auto elapsedMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFpsReport).count();
        if (elapsedMs >= 1000)
        {
            double fps = (frameCount * 1000.0) / elapsedMs;
            std::cout << "FPS: " << std::fixed << std::setprecision(1) << fps
                      << " (" << frameCount << " frames in " << elapsedMs << "ms)"
                      << " [vulkan: " << width << "x" << height << "]"
                      << std::endl;
            frameCount = 0;
            lastFpsReport = now;
        }
    }

    return 0;
}

