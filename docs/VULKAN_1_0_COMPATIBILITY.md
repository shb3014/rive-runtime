# Rive Vulkan 1.0 兼容性分析

## 背景

PanVK (Mesa 的 Mali Bifrost/Valhall Vulkan 驱动) 当前仅报告 Vulkan 1.0:

```c
// mesa/src/panfrost/vulkan/panvk_vX_physical_device.c
static uint32_t get_api_version(void) {
    if (PAN_ARCH >= 10) return VK_API_VERSION_1_4;
    else return VK_API_VERSION_1_0;
}
```

Rive 渲染器在 `render_context_vulkan_impl.cpp:3245` 硬性要求 Vulkan >= 1.1，
导致无法在 PanVK + Mali-G52 (Bifrost, PAN_ARCH=7) 上使用 Vulkan 后端。

## 分析: Rive 实际使用的 Vulkan 特性

### Vulkan 1.0 核心 API (已满足)

| API | 用途 | 1.0 状态 |
|-----|------|----------|
| `vkCreateRenderPass` | 创建渲染 pass | 1.0 core |
| `vkCmdBeginRenderPass` | 开始渲染 | 1.0 core |
| `vkCmdNextSubpass` | 多 subpass 切换 | 1.0 core |
| `vkCmdEndRenderPass` | 结束渲染 | 1.0 core |
| `vkCmdPipelineBarrier` | 内存屏障 | 1.0 core |
| `vkCreateGraphicsPipelines` | 创建图形管线 | 1.0 core |
| `vkCmd{Draw,DrawIndexed}` | 绘制调用 | 1.0 core |
| Subpass self-dependencies | PLS 替代方案 | 1.0 core |
| `VkSubpassDependency` | 依赖声明 | 1.0 core |

### Vulkan 1.1 提升到核心的 API

| API | 1.1 Core | 1.0 KHR 回退 | Rive 状态 |
|-----|----------|--------------|-----------|
| `vkGetPhysicalDeviceFeatures2` | 1.1 core | `VK_KHR_get_physical_device_properties2` | **已有回退** (vulkan_instance.cpp:281-286) |
| `vkGetPhysicalDeviceProperties2` | 1.1 core | 同上 | 未直接使用 |
| `vkBindBufferMemory2` | 1.1 core | `VK_KHR_bind_memory2` | 未使用 |
| `vkGetBufferMemoryRequirements2` | 1.1 core | `VK_KHR_get_memory_requirements2` | VMA 内部可能使用 |

### 关键发现

1. **Rive 不使用 `vkCreateRenderPass2`** — 使用的是 1.0 的 `vkCreateRenderPass`
2. **Subpass 功能完全在 1.0 中可用** — `vkCmdNextSubpass`, `VkSubpassDependency` 都是 1.0 core
3. **唯一的 1.1 依赖** 是 `vkGetPhysicalDeviceFeatures2`，但 `VulkanInstance` 已经有 KHR 扩展回退
4. **VMA** 通过 `vulkanApiVersion` 参数正确处理 1.0

## 代码修改

### 修改点: `render_context_vulkan_impl.cpp:3245`

原代码:
```cpp
if (vk->physicalDeviceProperties().apiVersion < VK_API_VERSION_1_1)
{
    fprintf(stderr, "ERROR: ...\n");
    return nullptr;
}
```

修改后: 当 `RIVE_VK_ALLOW_1_0=1` 时允许 Vulkan 1.0 驱动通过版本检查，
输出警告而非错误。

### 不需要修改的代码

| 位置 | 检查 | 原因 |
|------|------|------|
| `line 759` | `apiVersion < VK_API_VERSION_1_3` | 厂商特定 workaround (ARM/IMG_TEC 复杂 render pass 限制)，对 1.0 自动生效 |
| `line 876` | `apiVersion >= VK_API_VERSION_1_3` | IMG_TEC 特定的 raster ordering 条件，对 ARM 不影响 |
| `vulkan_instance.cpp:35` | `instanceVersion < VK_API_VERSION_1_1` | 实例版本处理，已有正确的 1.0 回退 |

## PanVK 已知限制

| 特性 | 支持状态 | 影响 |
|------|----------|------|
| `fragmentStoresAndAtomics` | 可能支持 | 需验证; 如不支持则 atomic mode 不可用 |
| `independentBlend` | 可能支持 | 需验证 |
| `EXT_rasterization_order_attachment_access` | 可能不支持 | ARM 有 subpass self-dependency 回退 |
| `VK_EXT_fragment_shader_interlock` | 可能不支持 | clockwise mode 不可用 |

## 测试方法

```bash
# 1. 确保 PanVK 驱动可用
VK_ICD_FILENAMES=/path/to/panvk_icd.json vulkaninfo

# 2. 运行 Rive Vulkan 后端 (需要 Vulkan player, 非当前 GL player)
RIVE_VK_ALLOW_1_0=1 ./vulkan_player dress-up.riv

# 3. 检查 PanVK 支持的特性
vulkaninfo --summary
```

## 风险评估

| 风险 | 级别 | 说明 |
|------|------|------|
| API 兼容性 | **低** | 所有使用的 API 都是 1.0 core |
| VMA 兼容性 | **低** | VMA 正确处理 apiVersion=1.0 |
| PanVK 驱动成熟度 | **中** | PanVK 仍在开发中，可能有 bug |
| 特性缺失 | **中** | 某些 Vulkan 特性 PanVK 可能未实现 |
| Shader 编译 | **高** | PanVK 的 SPIR-V 编译器可能对复杂 shader 有问题 |

## 建议

1. **短期**: 使用 `RIVE_VK_ALLOW_1_0=1` 在 PanVK 上进行初步测试
2. **中期**: 如果测试通过，考虑将该标志默认启用或完全移除版本检查
3. **长期**: 等待 PanVK 上游支持 Vulkan 1.1+（取决于 Mesa 开发进度）
4. **上游合并**: 此修改对现有行为无侵入性（需要显式 opt-in），适合向 Rive 上游提交
