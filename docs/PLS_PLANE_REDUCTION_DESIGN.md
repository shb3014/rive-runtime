# 方案 D: Per-Flush PLS 平面数选择机制 — 设计文档

## 1. 现状分析

### 1.1 当前 PLS 平面布局 (4 planes, 16 bytes/pixel)

```
constants.glsl:172-176:
  #define COLOR_PLANE_IDX           0   // rgba8   (4 bytes)
  #define CLIP_PLANE_IDX            1   // r32ui   (4 bytes)
  #define SCRATCH_COLOR_PLANE_IDX   2   // rgba8   (4 bytes)
  #define COVERAGE_PLANE_IDX        3   // r32ui   (4 bytes)
  #define PLS_PLANE_COUNT           4   // 总计: 16 bytes/pixel
```

### 1.2 各平面的使用条件

| 平面 | 格式 | rasterOrdering 模式 | clockwise 模式 | 条件依赖 |
|------|------|---------------------|----------------|----------|
| `colorBuffer` | rgba8 | **始终读写** | 仅非 fixed-function 时 | 无条件 (rasterOrdering) |
| `clipBuffer` | r32ui | 有 clipping 时读写, 否则 PRESERVE | 有 clipping 时读写 | `ENABLE_CLIPPING` |
| `scratchColorBuffer` | rgba8 | 始终读写* | 仅 advanced blend 时 | 见下方分析 |
| `coverageCountBuffer` | r32ui | **始终读写** | **始终读写** | 无条件 |

*`scratchColorBuffer` 在 rasterOrdering 模式的详细用途:

1. **非首次 fragment**: 存储 `dstColorPremul` 以便后续 fragment 读取 (line 170-179)
2. **嵌套裁剪** (`ENABLE_NESTED_CLIPPING`): 存储 `outerClipCoverage` (line 108-121)
3. **Advanced blend** (`ENABLE_ADVANCED_BLEND`): 无额外使用 (clockwise 模式)

### 1.3 已有的动态平面计数基础设施

`render_context.cpp:930-958` 中 `pls_transient_backing_plane_count()` **已经** 为 clockwise
模式做了动态平面计数:

```cpp
case gpu::InterlockMode::clockwise:
{
    uint32_t n = 1; // coverage (always)
    if (combinedDrawContents & (activeClip | clipUpdate))
        ++n; // clip
    if (combinedDrawContents & advancedBlend)
        ++n; // scratch color
    return n;
}
```

但 rasterOrdering 模式始终返回 `3` (clip + scratch + coverage), **不做动态选择**。

### 1.4 Shader 侧的问题

即使某个平面不被使用, shader 仍然声明它并执行 `PLS_PRESERVE` 操作:

```glsl
// draw_raster_order_path.frag — 无论是否有 clipping, 都会执行:
PLS_PRESERVE_UI(clipBuffer);         // line 215
// 无论是否嵌套裁剪, scratchColorBuffer 都在声明块中
PLS_PRESERVE_4F(scratchColorBuffer); // line 179
```

`PLS_PRESERVE` 在 Bifrost 上编译为 tile memory 中的 no-op, 但 **PLS 平面声明本身** 会
占用 tile memory 带宽 (每像素 4 bytes per plane, 影响 tile 并行度)。

## 2. 可行的缩减模式

### 模式 A: 3 planes (无嵌套裁剪) — 12 bytes/pixel (-25%)

移除 `scratchColorBuffer`, 仅在以下条件全部满足时适用:

- 无 `ENABLE_NESTED_CLIPPING`
- 无 `ENABLE_ADVANCED_BLEND` (rasterOrdering 模式)
- **问题**: rasterOrdering 模式中 `scratchColorBuffer` 用于存储 dstColor 跨 fragment
  共享。如果路径的多个 fragment 覆盖同一像素, 第二个 fragment 需要读取 scratchColorBuffer
  中存储的原始 dstColor。**移除此平面会破坏 rasterOrdering 的正确性。**

**结论: 模式 A 在 rasterOrdering 中不可行。** 仅在 clockwise 模式中可行 (且已通过
`pls_transient_backing_plane_count` 部分实现)。

### 模式 B: 3 planes (无裁剪) — 12 bytes/pixel (-25%)

移除 `clipBuffer`, 适用条件:

- 无 `ENABLE_CLIPPING` 且无 `ENABLE_NESTED_CLIPPING`

**可行性: 中等。** 需要:
1. 新的 shader 变体 (不声明 clipBuffer)
2. 重新映射 `COVERAGE_PLANE_IDX` 从 3 改为 2
3. 修改 PLS 初始化代码

### 模式 C: 2 planes (clockwise + fixed function) — 8 bytes/pixel (-50%)

**已存在于代码中!** `pls_impl_ext_native.cpp:122`:
```cpp
glFramebufferPixelLocalStorageSizeEXT(GL_FRAMEBUFFER, 2 * sizeof(uint32_t));
```

但受 `EXT_shader_pixel_local_storage2` 扩展限制 (当前设备不支持)。

## 3. 推荐方案: 渐进式平面缩减

### 第一阶段: 利用已有 clockwise 动态计数 (低风险, 1-2 天)

**目标**: 确保 clockwise 模式的动态平面计数在 EXT native 路径中生效

**当前问题**: `pls_transient_backing_plane_count()` 返回正确的数量, 但
`pls_impl_ext_native.cpp` 的标准路径 (非 fixed-function, 非 workaround) **不调用
`glFramebufferPixelLocalStorageSizeEXT()`**, 而是依赖 shader 声明隐式确定大小。

**修改点**:

```cpp
// pls_impl_ext_native.cpp - activatePixelLocalStorage()
void activatePixelLocalStorage(RenderContextGLImpl* impl,
                               const FlushDescriptor& desc) override
{
    // ...existing code...
    if (desc.fixedFunctionColorOutput)
    {
        // ...existing 2-plane path...
    }
+   else if (desc.interlockMode == gpu::InterlockMode::clockwise)
+   {
+       // Clockwise mode may use fewer PLS planes depending on content.
+       // Use EXT_shader_pixel_local_storage2 to set the exact plane count
+       // if available, to reduce tile memory bandwidth.
+       uint32_t planeCount = /* from desc or compute from combinedDrawContents */;
+       if (impl->m_capabilities.EXT_shader_pixel_local_storage2 &&
+           planeCount < PLS_PLANE_COUNT)
+       {
+           glFramebufferPixelLocalStorageSizeEXT(GL_FRAMEBUFFER,
+                                                 planeCount * sizeof(uint32_t));
+       }
+   }
    else if (impl->m_capabilities.usePixelLocalStorage2AsWorkaround)
    {
        // ...existing workaround path...
    }
    // ...rest of function...
}
```

**注意**: 这需要 `EXT_shader_pixel_local_storage2`, 当前设备不支持。

### 第二阶段: Shader 变体 — 条件性移除 clipBuffer (中等风险, 1-2 周)

**目标**: 当 flush 不需要裁剪时, 使用只有 3 个平面的 shader 变体

**修改涉及的文件**:

| 文件 | 修改内容 |
|------|----------|
| `constants.glsl` | 新增 `PLS_PLANE_COUNT_NO_CLIP = 3`, 条件化 `COVERAGE_PLANE_IDX` |
| `draw_raster_order_path.frag` | 条件编译 clipBuffer 声明和 PRESERVE |
| `draw_clockwise_path.frag` | 同上 |
| `pls_load_store_ext.glsl` | 条件编译 clipBuffer 的 clear/load/store |
| `pls_impl_ext_native.cpp` | 根据 shader features 选择平面数 |
| `render_context.cpp` | 传递 plane count 信息给 FlushDescriptor |

**Shader 修改示例** (`draw_raster_order_path.frag`):

```glsl
PLS_BLOCK_BEGIN
PLS_DECL4F(COLOR_PLANE_IDX, colorBuffer);
#ifdef @ENABLE_CLIPPING
PLS_DECLUI(CLIP_PLANE_IDX, clipBuffer);
#endif
PLS_DECL4F(SCRATCH_COLOR_PLANE_IDX, scratchColorBuffer);
// When clipping is disabled, coverage index shifts down
#ifdef @ENABLE_CLIPPING
PLS_DECLUI(COVERAGE_PLANE_IDX, coverageCountBuffer);
#else
PLS_DECLUI(SCRATCH_COLOR_PLANE_IDX + 1, coverageCountBuffer);
#endif
PLS_BLOCK_END
```

**问题**: PLS 平面索引必须是编译时常量。如果 clipBuffer 不存在,
`SCRATCH_COLOR_PLANE_IDX` 和 `COVERAGE_PLANE_IDX` 需要重新映射。这会导致:

1. **Shader 变体爆炸**: `ENABLE_CLIPPING × ENABLE_NESTED_CLIPPING × ...`
   每增加一个条件平面配置, shader 变体数量翻倍
2. **EXT PLS 兼容性**: `EXT_shader_pixel_local_storage` (无 2) 不支持
   `glFramebufferPixelLocalStorageSizeEXT()`, 平面数由 shader 隐式决定。
   如果 shader 声明了 3 个平面, 驱动会自动使用 3 个平面。
   **这意味着仅通过 shader 变体就能实现, 不需要 PLS2 扩展!**

### 第三阶段: RasterOrdering 模式的 scratchColorBuffer 消除 (高风险, 2-4 周)

**问题核心**: `scratchColorBuffer` 在 rasterOrdering 模式中承担两个关键角色:

1. 存储首次 fragment 的 dstColor, 供后续 fragment 使用 (避免多次读取已被覆盖的 colorBuffer)
2. 嵌套裁剪时存储 outerClipCoverage

**替代方案 (理论探讨)**:

方案 3a: 将 dstColor 编码到 coverage 中
- 不可行: coverage 是 r32ui (4 bytes), 无法存储 rgba8 数据

方案 3b: 使用 interior triangles only 模式
- 如果保证每个像素只有一个 fragment (interior triangles), scratchColorBuffer 不需要
- 但 edge fragments 仍需要多次访问
- 不完全可行

方案 3c: 两 pass 渲染
- Pass 1: 累积所有 coverage (只写 coverageBuffer)
- Pass 2: 基于最终 coverage 一次性混合
- 代价: 额外一次全屏绘制 + 额外一个 pass 的带宽
- 可能反而更慢

**结论: 第三阶段在 rasterOrdering 模式中不推荐。**

## 4. 实现路径图

```
Phase 1: Clockwise 平面优化               Phase 2: No-Clip Shader 变体
[1-2 天]                                  [1-2 周]
                                          
├─ 验证 clockwise 模式的视觉正确性         ├─ 修改 draw_raster_order_path.frag
│  (方案 B 测试结果)                       │  条件移除 clipBuffer 声明
│                                         │
├─ 如果 PLS2 可用:                        ├─ 修改 pls_load_store_ext.glsl
│  └─ 在 clockwise 路径中调用              │  条件移除 clipBuffer clear
│     glFramebufferPixelLocalStorageSizeEXT│
│                                         ├─ 修改 constants.glsl
├─ 如果 PLS2 不可用 (当前情况):            │  添加 NO_CLIP 平面布局
│  └─ 修改 draw_clockwise_path.frag       │
│     条件移除不需要的平面声明              ├─ 修改 render_context.cpp
│     (shader 声明决定平面数)               │  传递 no-clip 标志给 shader
│                                         │
└─ 测试 FPS 变化                          └─ 测试 + 验证正确性
```

## 5. 上游合并可行性评估

### 有利因素

1. **clockwise 模式的动态计数已存在**: `pls_transient_backing_plane_count()` 是上游代码,
   证明 Rive 团队认可动态平面选择的概念
2. **`fixedFunctionColorOutput` 已实现 2-plane 模式**: 2-plane 路径是完整的、经过测试的
3. **Shader feature flags 基础设施完善**: `ENABLE_CLIPPING`, `ENABLE_NESTED_CLIPPING` 等
   已用于 shader 变体管理, 添加条件平面声明是自然扩展
4. **不影响其他平台**: 条件编译只在特定 shader 变体中生效, 默认行为不变

### 不利因素

1. **增加 shader 变体数量**: 每个新条件翻倍变体数。当前已有 `2^7 = 128` 种组合
   (7 个 ShaderFeatures 标志位), 新增 plane reduction 标志会翻倍到 256
2. **EXT PLS 路径特异性**: 平面数优化主要受益于 `EXT_shader_pixel_local_storage` 路径,
   这是嵌入式/移动端特有路径。桌面端使用 `framebuffer_fetch` 或 Vulkan subpass,
   不受此影响
3. **测试覆盖度**: 需要覆盖所有 `[clipping × nested_clipping × advanced_blend ×
   plane_count]` 组合

### 建议的上游策略

1. **先提交 clockwise shader 条件平面**: 这是最小侵入性的改动, 仅影响 clockwise 模式的
   `draw_clockwise_path.frag`, 且已有对应的动态计数逻辑
2. **作为 `EXT_shader_pixel_local_storage` 优化**: 在 PR 中明确标注这是移动端/嵌入式
   优化, 对桌面端无影响
3. **附带 Mali-G52 性能数据**: 测量有/无平面缩减的 FPS 差异作为上游合并依据

## 6. 性能预期

| 配置 | 平面数 | bytes/pixel | 预期 FPS 变化 (500×500) |
|------|--------|-------------|------------------------|
| 当前 rasterOrdering | 4 | 16 | 基线 (~47 FPS) |
| 无 clip (rasterOrdering) | 3 | 12 | +10-15% (~52-54 FPS) |
| clockwise (无 clip, 无 advanced blend) | 1 | 4 | +30-50% (需要验证) |
| clockwise + fixed function | 2 | 8 | +25-40% (需 PLS2) |

注: 实际收益取决于 Mali-G52 tile memory 实现和 Mesa/Panfrost 驱动对 PLS 平面数变化的响应。
如果 tile memory 分配是固定的 (不随 PLS 声明变化), 则收益可能很小。
需要通过实际测试 (方案 E 的 shader codegen 分析) 来确认。

## 7. 下一步行动

1. 运行方案 E (Mesa 诊断), 确认 Bifrost shader 编译中 PLS 平面数是否影响 tile 大小
2. 运行方案 B (clockwise 测试), 确认 clockwise 模式在目标内容上的正确性
3. 如果两者都正面, 优先实现 Phase 1 (clockwise shader 条件平面)
4. 测量性能差异, 决定是否继续 Phase 2
