# RFC: 静态层缓存 (Static Layer Cache)

**状态**: 草案 (Draft)
**目标平台**: 嵌入式 Mali GPU (Bifrost/Valhall), 特别是 Mali-G52 + RK3566
**预期收益**: 50-80% 减少 fragment 工作量 (取决于内容)

---

## 1. 动机

### 1.1 问题

在 Mali-G52 (RK3566) 上以 500×500 分辨率渲染 `dress-up.riv` 时, FPS 约 47,
受限于 fragment/PLS 带宽。当前渲染器每帧完整重绘所有内容, 包括帧间未变化的静态元素。

对于大多数 Rive 动画, 只有一小部分 drawable (如角色动画) 在帧间变化,
大量背景元素 (装饰、文字、静态图形) 保持不变。如果能将这些静态元素缓存到纹理,
每帧只重绘变化的部分, 可以显著减少 PLS 操作次数和 fragment 着色工作量。

### 1.2 性能模型

假设 `dress-up.riv` 中 30% 的像素由动画 drawable 覆盖, 70% 是静态背景:

```
当前: 500×500 × 4 PLS planes × ~8 ops/fragment = ~8M PLS 操作
目标: 500×500 × 0.30 × 4 × ~8 + 500×500 × 1 (纹理采样) = ~2.65M 操作
预期加速: ~3× 减少 PLS 操作
```

## 2. 设计概述

### 2.1 架构

```
┌─────────────────────────────────────────┐
│              Artboard                    │
│  ┌─────────────────────────────────────┐ │
│  │     Drawable Linked List            │ │
│  │  [D1] → [D2] → ... → [Dn]         │ │
│  └─────────────────────────────────────┘ │
│                    │                     │
│         ┌─────────┴─────────┐           │
│         ▼                   ▼           │
│   ┌───────────┐    ┌──────────────┐     │
│   │  Static   │    │   Dynamic    │     │
│   │ Drawables │    │  Drawables   │     │
│   └─────┬─────┘    └──────┬───────┘     │
│         │                  │            │
│         ▼                  ▼            │
│   ┌───────────┐    ┌──────────────┐     │
│   │  Cached   │    │  Live PLS    │     │
│   │  Texture  │    │  Rendering   │     │
│   └─────┬─────┘    └──────┬───────┘     │
│         │                  │            │
│         └────────┬─────────┘            │
│                  ▼                      │
│         ┌──────────────┐                │
│         │  Composite   │                │
│         │    Pass      │                │
│         └──────────────┘                │
└─────────────────────────────────────────┘
```

### 2.2 核心组件

| 组件 | 职责 |
|------|------|
| `DirtTracker` | 跟踪 drawable 的 `ComponentDirt` 变化, 识别静态/动态 |
| `LayerPartitioner` | 将 drawable 链表分割为连续的静态/动态 "层" |
| `TextureCache` | 管理缓存纹理的分配、渲染和失效 |
| `CompositePass` | 将缓存纹理与动态层合成到最终帧缓冲 |

## 3. 详细设计

### 3.1 变化检测 (`DirtTracker`)

#### 3.1.1 Rive 现有的变化检测机制

Rive 已有 `ComponentDirt` 标志系统:

```cpp
// include/rive/component_dirt.hpp
enum class ComponentDirt : uint16_t
{
    Path           = 1 << 0,   // 路径几何变化
    Transform      = 1 << 1,   // 本地变换变化
    WorldTransform = 1 << 2,   // 世界变换变化
    Paint          = 1 << 3,   // 绘制属性变化 (颜色, 渐变等)
    RenderOpacity  = 1 << 4,   // 不透明度变化
    DrawOrder      = 1 << 5,   // 绘制顺序变化
    Clipping       = 1 << 6,   // 裁剪区域变化
    // ...
};
```

此外, `Artboard::drawOrderChangeCounter()` 在绘制顺序改变时递增。

#### 3.1.2 当前限制

`ComponentDirt` 在每次 `advanceAndApply()` 调用中被设置然后清除。
**没有持久化的 "此 drawable 在上一帧中被修改" 信息。**

#### 3.1.3 提议的 `DirtTracker`

```cpp
// 新增类, 位于 player 层 (非 Rive 核心)
class DirtTracker
{
public:
    // 在 advanceAndApply() 前后调用
    void beginFrame(Artboard* artboard);
    void endFrame();

    // 查询某个 drawable 是否在本帧被修改
    bool isDrawableDirty(const Drawable* drawable) const;

    // 查询整体是否有变化
    bool hasAnyChange() const;

    // 获取静态帧计数 (连续未变化的帧数)
    uint32_t staticFrameCount() const;

private:
    // 方案 A: 基于 ComponentDirt 拦截 (需要 Rive 上游修改)
    //   在 Component::addDirt() 中添加回调
    //
    // 方案 B: 基于快照比较 (不需要上游修改)
    //   每帧记录所有 drawable 的 transform hash + paint hash
    //   与上一帧比较来判断变化
    //
    // 方案 C: 基于 drawOrderChangeCounter + 全局脏标志
    //   粗粒度: 任何变化都标记整个 artboard 为脏

    struct DrawableSnapshot
    {
        uint64_t transformHash;
        uint64_t paintHash;
        uint64_t pathHash;
    };

    std::unordered_map<const Drawable*, DrawableSnapshot> m_prevSnapshots;
    std::unordered_map<const Drawable*, DrawableSnapshot> m_currSnapshots;
    uint32_t m_staticFrameCount = 0;
};
```

**方案选择权衡**:

| 方案 | 精度 | 性能开销 | 上游依赖 |
|------|------|----------|----------|
| A: ComponentDirt 回调 | 高 | 低 | 需要上游 PR |
| B: 快照比较 | 高 | 中 (hash 计算) | 无 |
| C: 全局脏标志 | 低 (全有或全无) | 极低 | 无 |

**推荐**: 先用方案 C 做 PoC, 验证可行性后用方案 B 精细化, 最后考虑方案 A 上游合并。

### 3.2 层分割 (`LayerPartitioner`)

#### 3.2.1 分层策略

将 drawable 链表按连续性分割为 "层", 每一层内的所有 drawable 要么全部静态, 要么全部动态:

```
Drawable 链表: [D1] → [D2] → [D3] → [D4] → [D5] → [D6] → [D7]
                S      S      D      D      S      S      S
                └──┬──┘ └──┬──┘ └─────┬─────┘
                静态层0   动态层1    静态层2

其中: S = 静态, D = 动态 (本帧有变化)
```

#### 3.2.2 分层数据结构

```cpp
struct CacheLayer
{
    enum Type { Static, Dynamic };

    Type type;
    Drawable* firstDrawable;  // 指向层内第一个 drawable
    Drawable* lastDrawable;   // 指向层内最后一个 drawable
    uint32_t drawableCount;

    // 仅 Static 层使用
    struct {
        GLuint cachedTexture = 0;
        bool valid = false;            // 缓存是否有效
        uint32_t lastRenderedFrame = 0; // 最后渲染到缓存的帧号
        AABB bounds;                   // 层的边界 (用于合成)
    } cache;
};
```

#### 3.2.3 层合并优化

过多的层会增加合成开销。设定阈值:
- 最大层数: 8 (超出则合并相邻同类型层或降级为无缓存)
- 最小层 drawable 数: 如果一个静态层只有 1 个简单 drawable, 直接归入动态层

### 3.3 纹理缓存 (`TextureCache`)

#### 3.3.1 缓存纹理分配

```cpp
class TextureCache
{
public:
    // 为一个静态层渲染到缓存纹理
    void renderLayerToCache(RenderContext* ctx,
                            const CacheLayer& layer,
                            uint32_t width,
                            uint32_t height);

    // 检查缓存是否有效
    bool isCacheValid(const CacheLayer& layer) const;

    // 失效并释放缓存
    void invalidate(CacheLayer& layer);

private:
    // 纹理池 — 避免频繁分配/释放
    struct CachedTexture
    {
        GLuint textureId;
        GLuint fboId;        // 用于渲染到此纹理的 FBO
        uint32_t width;
        uint32_t height;
        bool inUse;
    };

    std::vector<CachedTexture> m_pool;
    uint32_t m_maxPoolSize = 4; // 最多缓存 4 个纹理
};
```

#### 3.3.2 缓存渲染流程

将静态层渲染到缓存纹理需要一次完整的 Rive PLS render pass:

```
1. 绑定缓存纹理的 FBO 为渲染目标
2. beginFrame() — 使用缓存纹理尺寸
3. 遍历层内的 drawable, 调用 draw()
4. flush() — 执行 PLS 渲染
5. 标记缓存为有效
```

**关键问题**: Rive 的 `RenderContext` 是为整个 artboard 设计的, 单独渲染 drawable 子集
需要:
- 在 `artboard->draw(renderer)` 之前修改 drawable 链表 (只暴露目标层的 drawable)
- 或者在 player 层手动遍历 drawable 并调用 `drawable->draw(renderer)`
- 后者需要正确处理裁剪状态和变换栈

#### 3.3.3 缓存失效策略

| 触发条件 | 动作 |
|----------|------|
| 层内任何 drawable 的 ComponentDirt 被设置 | 标记缓存无效 |
| `drawOrderChangeCounter` 改变 | 重新分层, 所有缓存无效 |
| 窗口尺寸改变 | 所有缓存无效 |
| 显式失效 API 调用 | 特定缓存无效 |

### 3.4 合成 (`CompositePass`)

#### 3.4.1 合成策略

合成需要将缓存纹理和动态层的渲染结果按正确顺序混合:

```
最终帧 = 遍历层列表:
  对每个静态层: 将缓存纹理 blit 到帧缓冲 (src-over blend)
  对每个动态层: 执行完整 PLS 渲染到帧缓冲
```

**问题**: Rive PLS 渲染假设从空白帧开始 (或 LoadAction::preserveRenderTarget)。
动态层需要在已有静态层内容之上渲染, 这要求:

1. `LoadAction::preserveRenderTarget` — 保留先前内容
2. 正确的裁剪区域 — 只在动态层的边界内渲染

#### 3.4.2 合成着色器

```glsl
// composite.frag — 简单纹理混合
#version 300 es
precision mediump float;
in vec2 v_texCoord;
out vec4 fragColor;
uniform sampler2D u_cachedTexture;

void main()
{
    fragColor = texture(u_cachedTexture, v_texCoord);
}
```

#### 3.4.3 多层合成顺序

```
Frame rendering:
  1. Clear framebuffer
  2. For layer in layers (back to front):
     if layer.type == Static && layer.cache.valid:
       Blit layer.cache.texture to framebuffer (src-over)
     elif layer.type == Static && !layer.cache.valid:
       Render layer drawables via PLS, save to cache
       Blit cache to framebuffer
     elif layer.type == Dynamic:
       Render layer drawables via PLS directly to framebuffer
       (with LoadAction::preserveRenderTarget)
```

## 4. Rive 上游兼容性分析

### 4.1 需要上游修改的部分

| 项目 | 上游修改类型 | 难度 | 必要性 |
|------|-------------|------|--------|
| Drawable 链表子集渲染 | Artboard API 扩展 | 中 | **必须** |
| ComponentDirt 回调 | Component 内部修改 | 中 | 可选 (方案 A) |
| RenderContext 多 pass 支持 | RenderContext API 扩展 | 高 | **必须** |
| LoadAction 支持 | 已存在 | - | - |

### 4.2 可在 Player 层实现的部分

| 项目 | 实现位置 | 对上游无侵入性 |
|------|----------|---------------|
| DirtTracker (方案 B/C) | Player 层 | 是 |
| LayerPartitioner | Player 层 | 是 |
| TextureCache | Player 层 (GL calls) | 是 |
| CompositePass | Player 层 (GL calls) | 是 |

### 4.3 最大的上游障碍

**`Artboard::draw()` 不支持选择性渲染 drawable 子集。**

当前 `draw()` 遍历完整的 drawable 链表:

```cpp
// src/artboard.cpp:1394 (概略)
void Artboard::draw(Renderer* renderer)
{
    // Draw background paints
    for (auto& paint : m_ShapePaints) { ... }
    // Draw all drawables
    for (Drawable* d = m_FirstDrawable; d != nullptr; d = d->prev) {
        d->draw(renderer);
    }
}
```

**解决方案**:

1. **侵入式**: 向 `Artboard` 添加 `drawRange(Renderer*, Drawable* first, Drawable* last)`
2. **非侵入式**: 在 Player 层直接遍历 drawable 链表, 跳过 `Artboard::draw()`:

```cpp
// Player 层伪代码
void renderLayer(RiveRenderer* renderer, CacheLayer& layer)
{
    renderer->save();
    // 设置变换和裁剪
    for (Drawable* d = layer.firstDrawable;
         d != nullptr && d != layer.lastDrawable->prev;
         d = d->prev)
    {
        d->draw(renderer);
    }
    renderer->restore();
}
```

**风险**: 直接遍历 drawable 链表可能遗漏 `Artboard::draw()` 中的裁剪代理 drawable
(`ClippingShape` proxy)。需要确保裁剪状态正确传播。

## 5. 实现计划

### Phase 0: 可行性验证 (1 周)

**目标**: 验证 "缓存一帧完整渲染结果, 后续帧直接使用" 的性能收益

1. 在 `rk3566_drm_player.cpp` 中添加简单的帧缓存:
   - 第一帧: 正常渲染 + 将结果拷贝到缓存纹理
   - 后续帧: 直接 blit 缓存纹理 (不调用 Rive 渲染)
2. 测量 FPS 差异: 如果缓存 blit 是 60 FPS, 确认瓶颈在 PLS 渲染

**预期结果**: 缓存 blit 应该远超 60 FPS, 验证 PLS 确实是瓶颈

### Phase 1: 全局脏检测 (1 周)

**目标**: 跳过未变化帧的重新渲染

1. 实现 `DirtTracker` 方案 C (全局脏标志)
2. 如果帧间无变化, 复用上一帧的缓存纹理
3. 适用场景: 动画暂停时, 交互空闲时

### Phase 2: 两层分割 (2-3 周)

**目标**: 将 artboard 分为 "背景" 和 "前景" 两层

1. 实现 `DirtTracker` 方案 B (快照比较)
2. 将 drawable 链表分为静态组和动态组
3. 静态组渲染到缓存纹理
4. 每帧: blit 背景 + PLS 渲染前景
5. 需要处理层间裁剪关系

### Phase 3: 多层分割 + 上游集成 (4-6 周)

**目标**: 通用的多层缓存机制

1. 实现完整的 `LayerPartitioner` (8 层上限)
2. 与 Rive 上游讨论 `Artboard::drawRange()` API
3. 处理裁剪代理 drawable 的跨层问题
4. 实现纹理池回收
5. 性能自适应: 如果缓存开销 > 收益, 自动降级

## 6. 风险和缓解

| 风险 | 级别 | 缓解 |
|------|------|------|
| 裁剪状态跨层不正确 | **高** | Phase 0 验证裁剪场景; 保守回退 (有裁剪时不分层) |
| 缓存纹理内存开销 | **中** | 纹理池限制 (4 纹理上限); 仅在内存充足时启用 |
| 变化检测精度不足 | **中** | 从粗粒度 (方案 C) 逐步精细化 (方案 B → A) |
| 上游不接受分层渲染 API | **中** | Phase 2 可在 Player 层完全实现, 不依赖上游 |
| 合成 pass 开销抵消收益 | **低** | 简单 blit 在 Mali-G52 上极快 (纹理采样 vs PLS 多次读写) |
| 半透明 drawable 的层间混合不正确 | **高** | 确保层间使用 src-over premultiplied alpha |

## 7. 与 Rive 上游的讨论要点

### 7.1 API 提案

```cpp
// 最小化的上游 API 扩展提案
class Artboard
{
public:
    // 现有
    void draw(Renderer* renderer);

    // 新增: 渲染指定范围的 drawables
    void drawRange(Renderer* renderer,
                   Drawable* first,
                   Drawable* last);

    // 新增: 访问 drawable 链表 (已有 firstDrawable, 但无 public 遍历)
    Drawable* firstDrawable() const;

    // 新增: 可选 — 暴露每个 drawable 的变化状态
    bool isDrawableDirty(const Drawable* drawable) const;
};
```

### 7.2 上游可能的反馈

| 点 | 可能的反馈 | 应对 |
|----|-----------|------|
| "不想暴露 drawable 链表细节" | 提供 iterator 接口代替 | 设计为 range-based for 兼容 |
| "变化检测应该在渲染器层" | 在 `RenderContext` 中添加缓存层 | 更复杂, 但更通用 |
| "这是应用层的事, 不属于 SDK" | 保持为外部扩展 | 用 Player 层方案 (Phase 2) |
| "对 WebAssembly/移动端也有用" | 作为通用性能优化 | 提供跨平台性能数据 |

### 7.3 建议的沟通方式

1. 先在 Rive Community Discord 或 GitHub Issues 中提出讨论
2. 附带 Mali-G52 的性能数据 (Phase 0 结果)
3. 以 "嵌入式优化" 定位, 不影响现有 API 的默认行为
4. 提供完整的 Phase 0 PoC 代码作为参考实现

## 8. 替代方案对比

| 方案 | 性能收益 | 实现复杂度 | 通用性 | 上游兼容 |
|------|----------|-----------|--------|----------|
| 静态层缓存 (本 RFC) | **50-80%** | **高** | 中 | 需讨论 |
| 动态分辨率 (方案 A) | 30-40% | 低 | 高 | 无需 |
| Clockwise 模式 (方案 B) | 20-40% | 极低 | 高 | 已支持 |
| PLS 平面缩减 (方案 D) | 10-25% | 中 | 中 | 可行 |

**建议**: 静态层缓存是长期终极方案, 但短期应先实施方案 A/B/D 获取即时收益,
同时进行 Phase 0 验证缓存方案的可行性。

## 9. 附录: dress-up.riv 内容分析 (预估)

| 类别 | 预估占比 | 变化频率 | 缓存适用性 |
|------|----------|----------|-----------|
| 背景装饰 | ~40% | 从不 | **高** |
| UI 元素 (按钮等) | ~20% | 仅交互时 | 高 |
| 角色身体 | ~25% | 每帧 | 低 (动态) |
| 角色配饰动画 | ~15% | 每帧 | 低 (动态) |

预期: 缓存背景+UI 层 (~60% 像素) 可减少约 60% 的 PLS 渲染工作量。
