# ARM Mali Blob 驱动 — 完整评估与结论

> 最后更新: 2026-02-22
> 平台: RK3566 / Orange Pi 3B / Mali-G52 (Bifrost v7)
> 测试 blob: g2p0 + g13p0 + g24p0 (来自 Rockchip GitLab / tsukumijima)
> kbase: g25p0 (Rockchip BSP, 手动编译)

---

## 结论：Blob 驱动已可用于 dress-up.riv (g2p0 + g25p0 kbase)

**经过 kbase 升级 (g18p0→g25p0)、blob 版本选择 (g2p0) 和 DT 修复后，Blob 驱动实现正确渲染 + 完整调频。**

### 核心性能对比 (dress-up.riv, GPU @800MHz SCMI)

| 配置 | 384×384 | 500×500 | 600×600 | 700×700 | 1920×1280 | 渲染正确性 |
|------|---------|---------|---------|---------|-----------|----------|
| **⭐ g2p0 PLS 无限制** | **56** | **40** | **32** | **26** | **10** | **✅ (RMSE=5.5 vs Panfrost)** |
| g13p0 PLS limit=1 | 21 | 13 | 9 | 7 | 1.5 | ✅ (RMSE=11.2, 关键区域完美) |
| Panfrost @800MHz | 60 | 48 | 41 | 33 | 13 | ✅ 参考标准 |
| Panfrost @~400MHz | 35 | 26 | 21 | — | — | ✅ 参考标准 |
| g2p0 @~400MHz | 31 | 22 | 17 | — | — | ✅ |

### 关键发现 (2026-02-22)

1. **g25p0 kbase 修复了左眼 bug**: 之前 g18p0 kbase + blob 的左眼瞳孔缺失问题不再存在
2. **g2p0 blob 无 PLS tile pass 限制**: g13p0/g24p0 的 ~9 batch/pass 溢出是一个**版本回归**，g2p0 不受影响
3. **DT 修复已完成**: GPU OPP 和 devfreq 全面可用，6 频率点 (200-800MHz)，SCMI 时钟可控
4. **DT clock-names 修复**: `"gpu"→"clk_mali"` 让 kbase devfreq 正确关联 SCMI 时钟
5. **g2p0 @800MHz 比 Panfrost @800MHz 慢 ~17%**: 差距在可接受范围内

---

## 一、Blob 版本对比 (g2p0 vs g13p0 vs g24p0)

### PLS tile pass 限制

| blob DDK | PLS 单 pass batch 上限 | GPU 错误 (26 batch) | 来源 |
|----------|----------------------|-------------------|----|
| **g2p0** | **无限制 (26 batch OK)** | **0** | Rockchip GitLab |
| g13p0 | ~9 batch 后溢出 | 2869 (TRANSLATION_FAULT) | tsukumijima |
| g24p0 | ~9 batch 后溢出 | 2883 (JOB_READ_FAULT) | tsukumijima |

**g2p0 没有 PLS tile pass 限制 bug** — 这是 g13p0 中引入的版本回归。

### 渲染正确性 (vs Panfrost 参考帧, 所有结合 g25p0 kbase)

| blob + 模式 | RMSE vs Panfrost | 关键区域 (眼/瞳孔/帽/喙) | 左眼 bug |
|-------------|-----------------|-------------------------|---------|
| **g2p0 PLS 无限制** | **5.5** (500×500) | **全部 diff=0** | **✅ 已修复** |
| g13p0 PLS limit=1 | 11.2 (500×500) | 全部 diff=0 | ✅ 已修复 |
| g13p0 PLS limit=1 (g18p0 kbase) | N/A | ❌ 左眼瞳孔缺失 | ❌ 存在 |

RMSE 差异来源：帧间动画时间偏移和边缘抗锯齿差异，非渲染错误。

### MSAA 支持

| blob DDK | MSAA 报告 | 状态 |
|----------|----------|------|
| g2p0 | 异常值 (1098907648x) | ⚠️ 不可用 |
| g13p0 | 正常 | 65 batch 上限 (dress-up.riv 需 79) |
| g24p0 | 正常 | 65 batch 上限 |

g2p0 的 MSAA 不可用，但 PLS 无限制模式不需要 MSAA。

---

## 二、kbase 升级 (g18p0 → g25p0) 的关键修复

g25p0 kbase 从 Rockchip BSP 源码成功编译（通过大量兼容性补丁）:

| 修复项 | 说明 |
|--------|------|
| **左眼 bug** | g18p0 kbase 上 PLS limit=1 时猫头鹰左眼瞳孔缺失; g25p0 上完美 |
| 驱动命名冲突 | `KBASE_DRV_NAME` 从 `"mali"` 改为 `"mali_jm"` 避免与内置 CSF kbase 冲突 |
| 模块命名冲突 | Kbuild 中 `bifrost_kbase` 改为 `kbase_mali_jm` |
| 符号导出冲突 | 移除 `EXPORT_SYMBOL` / `EXPORT_TRACEPOINT_SYMBOL_GPL` 避免 modpost 错误 |
| 内核 API 兼容 | 安装 `version_compat_defs.h` 等 BSP 头文件到 Ubuntu 内核 |

---

## 三、设备树修复 (已完成 2026-02-22)

### 修复 1: GPU OPP 表 `rockchip,supported-hw` (✅ 已应用)

**问题**: `opp-table-1` (GPU) 中有空布尔属性 `rockchip,supported-hw;`，但各 OPP 条目缺少
`opp-supported-hw`。kbase 激活 OPP 过滤后所有 6 个频率点被拒绝，devfreq 完全失效。

**修复**: 直接修改 DTB，仅删除 GPU OPP 表 (opp-table-1) 的 `rockchip,supported-hw` 属性。
CPU/DMC/NPU 的 OPP 表保持不变（它们的 OPP 条目有对应的 `opp-supported-hw`）。

**结果**: GPU devfreq 恢复，6 个频率点全部可用 (200/300/400/600/700/800 MHz)。
CPU OPP 也正常 (408MHz~1800MHz)。

### 修复 2: GPU clock-names `"gpu"→"clk_mali"` (✅ 已应用)

**问题**: Ubuntu DT 的 GPU 节点 `clock-names = "gpu\0bus"` 使用上游命名。
kbase 的 `rockchip_init_opp_table()` 调用 `rockchip_init_opp_table(dev, info, "clk_mali", "mali")`
查找名为 `"clk_mali"` 的时钟，找不到后 SCMI devfreq target 无法正确关联时钟。

**修复**: 修改 DTB 中 GPU 节点的 `clock-names` 从 `"gpu\0bus"` 改为 `"clk_mali\0bus"`。
Panfrost 使用 `devm_clk_get(dev, NULL)` (索引方式获取第一个时钟)，不依赖名称 `"gpu"`，
因此此修改不影响 Panfrost。

**结果**: kbase devfreq 正确控制 SCMI 时钟 (`clk_scmi_gpu`)，频率变更生效。

### 关于 GPU 时钟架构的发现

RK3566 的 GPU 使用 SCMI (System Control and Management Interface) 管理时钟:
- `clk_scmi_gpu` — **真正的 GPU 核心时钟** (SCMI 管理，devfreq 控制此时钟)
- `clk_gpu` — 静态时钟 (固定 500MHz，不受 devfreq 影响)
- 通过 `dev_pm_opp_set_rate()` → SCMI 控制 `clk_scmi_gpu`

验证命令: `sudo cat /sys/kernel/debug/clk/clk_scmi_gpu/clk_rate`

### DTB 修改摘要

修改文件: `/lib/firmware/6.1.0-1025-rockchip/device-tree/rockchip/rk3566-orangepi-3b.dtb`
备份: `rk3566-orangepi-3b.dtb.bak-pre-opp-fix-*`

1. 删除 `opp-table-1` 中的 `rockchip,supported-hw;` (仅 GPU 表)
2. 修改 `gpu@fde60000` 的 `clock-names` 从 `"gpu\0bus"` 改为 `"clk_mali\0bus"`

---

## 四、完整 FPS 基准对比 (dress-up.riv)

> 所有测试均通过 SCMI 时钟验证真实 GPU 频率 (`clk_scmi_gpu`)

### @800MHz (SCMI 验证, g25p0 kbase, DT 已修复)

| 分辨率 | g2p0 PLS 无限制 | g13p0 PLS limit=1 | Panfrost PLS |
|--------|----------------|-------------------|-------------|
| 384×384 | **56** | 21 | **60** (vsync) |
| 500×500 | **40** | 13 | **48** |
| 600×600 | **32** | 9 | **41** |
| 700×700 | **26** | 7 | **33** |
| 1920×1280 | **10** | 1.5 | **13** |

### @~400MHz (SCMI 396MHz, 缩放验证)

| 分辨率 | g2p0 PLS 无限制 | Panfrost PLS |
|--------|----------------|-------------|
| 384×384 | 31 | 35 |
| 500×500 | 22 | 26 |
| 600×600 | 17 | 21 |

频率缩放线性验证: 800/396 ≈ 2.02x，性能差异 ≈ 1.8-1.9x，符合预期。

### 核心对比: g2p0 @800MHz vs Panfrost @800MHz

| 分辨率 | g2p0 PLS 无限制 | Panfrost PLS | 差异 |
|--------|----------------|-------------|------|
| 384×384 | 56 | 60 | **-7%** |
| 500×500 | 40 | 48 | **-17%** |
| 600×600 | 32 | 41 | **-22%** |
| 700×700 | 26 | 33 | **-21%** |
| 1920×1280 | 10 | 13 | **-23%** |

**g2p0 blob 比 Panfrost 慢约 17-23%。** 差距主要来自 PLS 实现效率差异。
在低分辨率 (384×384) 两者都接近 60 FPS vsync 上限。

### 关于之前文档中 "500MHz" 数据的修正

之前记录的 blob "500MHz" 测试数据实际上也是在 800MHz 下运行的 (通过 SCMI 时钟控制)。
`clk_gpu` (debugfs) 显示 500MHz 是一个静态分频器时钟，不是实际的 GPU 核心频率。
真正的核心时钟 `clk_scmi_gpu` 在 kbase devfreq performance governor 下始终为 800MHz。

---

## 五、视觉质量评估

### 像素级对比方法

对 dress-up.riv frame 5 捕获帧进行 Python RMSE 分析，检查 10 个关键区域
(center, top-face, bg-corner, bg-bottom, left-eye, right-eye, left-pupil, right-pupil, beak, hat)。

### 500×500 渲染分辨率

| 对比 | RMSE | >5 像素差异 | >50 像素差异 | 关键区域 |
|------|------|-----------|-----------|---------|
| g2p0 无限制 vs Panfrost | **5.5** | 1.7% | 0.3% | **全部 OK** |
| g13p0 limit=1 vs Panfrost | 11.2 | 2.5% | 0.8% | 全部 OK |
| g2p0 vs g13p0 | 9.7 | 1.3% | 0.4% | 全部 OK |

### 1920×1280 全分辨率

| 对比 | RMSE | >5 像素差异 | >50 像素差异 | 关键区域 |
|------|------|-----------|-----------|---------|
| g2p0 无限制 vs Panfrost | **9.9** | 3.3% | 0.9% | **全部 OK** (center 帧时间差) |
| g13p0 limit=1 vs Panfrost | 11.3 | 3.3% | 0.9% | 全部 OK (center 帧时间差) |

所有差异来源于**帧间动画时间偏移**和**边缘抗锯齿**，非渲染错误。
10 个关键区域在所有配置下**像素值完全一致** (diff=0)。

---

## 六、Blob 与 Panfrost 性能差距分析 (2026-02-22)

### 全面排查结论: 差距源于驱动内部代码质量

g2p0 blob @800MHz 比 Panfrost @800MHz 慢 ~17-23%。经全面检查后确认，**渲染路径和配置已完全相同，性能差距不可通过配置优化消除**。

### GL 能力对比

| 特性 | Panfrost (Mesa PLS) | g2p0 Blob | g13p0 Blob | 影响 |
|------|--------------------|-----------|-----------|----|
| GLES 版本 | 3.1 | 3.2 | 3.2 | — |
| PLS 路径 | EXTNative | EXTNative | EXTNative | **相同** |
| **PLS2** | ❌ | ❌ | ❌ | 两者都不可用 |
| **SSBO** | ❌ (VS blocks=0) | ❌ (VS blocks=0) | ❌ (VS blocks=0) | **硬件限制** |
| FB Fetch | EXT ✅ | ARM ✅ | ARM ✅ | 功能等价 |
| EXT_base_instance | ✅ | ❌ | ❌ | Panfrost 优势 |
| EXT_float_blend | ✅ | ❌ | ❌ | atlas 格式不同 |
| clip_cull_distance | ❌ | ❌ | ❌ | 相同 |
| blend_equation_adv | ✅ | ✅ | ✅ | 相同 |
| interlockMode | rasterOrdering | rasterOrdering | rasterOrdering | **相同** |
| atlasType | r32f | r16f | r16f | blob 更省带宽 |

### 关键发现

1. **PLS2 (`GL_EXT_shader_pixel_local_storage2`) 不可用**: 所有 Mali blob 和 Panfrost 均不支持。
   这是 Mali-G52 (Bifrost v7) 的驱动限制——PLS2 主要出现在更新的 Valhall GPU 上。

2. **SSBO 不可用 — 硬件限制**: Mali-G52 的 `MAX_VERTEX_SHADER_STORAGE_BLOCKS = 0`。
   Rive 渲染器要求顶点着色器 SSBO 支持（至少 4 个 blocks），两个驱动都退回到纹理 polyfill。

3. **Memory barrier 无性能影响**: 禁用 `RIVE_MALI_TESS_BARRIER=0` 和 `RIVE_MALI_RESOLVE_BARRIER=0`
   后 FPS 不变 (±0.5%)，且零 GPU 错误。G52 可以安全禁用这些 barrier。

4. **零 GPU 错误**: g2p0 运行期间无任何 translation/page fault。

5. **纯 GPU 吞吐量差异**: 在 ≤300×300 时两者都达到 60 FPS vsync 上限。
   差距仅在 GPU 成为瓶颈后出现，说明是着色器执行效率差异。

6. **g2p0 DDK 着色器编译器较老**: g2p0 是最早期的 Mali DDK 版本，
   Mesa NIR 编译器经过多年优化，在 GPU ISA 生成上更高效。

### 不可优化项

- PLS2 支持: 硬件/驱动限制，无法绕过
- SSBO 支持: 硬件限制 (Mali-G52 VS SSBOs = 0)，两个驱动均受影响
- 着色器编译器质量: 嵌入在 blob 二进制中，不可修改
- EXT_base_instance: blob 不暴露，无法绕过

---

## 七、Vulkan 可行性分析

### 结论: Vulkan 在 Mali-G52 上不可用于 Rive

| Vulkan 来源 | 版本 | fragmentStoresAndAtomics | Rive 兼容 |
|-------------|------|-------------------------|----------|
| **Blob (g2p0/g13p0/g24p0)** | **无 Vulkan** | — | ❌ |
| PanVK 系统 (Mesa 25.0) | 1.0 | false | ❌ (需 ≥1.1) |
| PanVK 自定义 (Mesa 26.1-dev) | 1.0 | false | ❌ (需 ≥1.1) |

- 所有 blob 是纯 GLES/EGL 库，不含任何 Vulkan 符号
- PanVK 对 Bifrost v7 (Mali-G52) 标记为 "not well-tested"，需 `PAN_I_WANT_A_BROKEN_VULKAN_DRIVER=1`
- PanVK 报告 Vulkan 1.0 且缺少 `fragmentStoresAndAtomics`，Rive Vulkan 后端无法使用
- Rockchip 官方也未为 RK3566 提供 Vulkan blob

---

## 八、新 Blob (g13p0/g24p0) 可行性测试 (2026-02-22)

### g13p0/g24p0 着色器编译器性能确认更优

| 配置 | 500×500 FPS | GPU Faults | 渲染正确 |
|------|------------|-----------|---------|
| **g13p0 limit=0** | **47.4** | 2944 | ❌ RMSE=207 |
| **g24p0 limit=0** | **47.8** | 2943 | ❌ RMSE=207 |
| g2p0 limit=0 | 40.3 | 0 | ✅ |
| g13p0 limit=1 | 12.6 | 0 | ✅ |

g13p0/g24p0 的着色器编译器产出确实更高效 (47 vs 40 FPS)，但 PLS bug 导致渲染完全损坏。

### PLS pass limit 精细扫描 (g13p0)

| Limit | FPS | GPU Faults | 状态 |
|-------|-----|-----------|------|
| 1 | 12.6 | 0 | ✅ 唯一正确选项 |
| 2 | 13.8 | 3088 | ❌ 损坏 |
| 3 | 17.0 | 3065 | ❌ 损坏 |
| 4 | 21.1 | 3075 | ❌ 损坏 |
| 5 | 23.8 | 3016 | ❌ 损坏 |
| 7 | 27.2 | 2989 | ❌ 损坏 |
| 8 | 29.8 | 3091 | ❌ 损坏 |
| 9 | 29.6 | 3090 | ❌ 损坏 |
| 12 | 34.6 | 3071 | ❌ 损坏 |

### PLS bug 根因分析

g13p0/g24p0 的 PLS bug 有**两层**:
1. **单 session 溢出**: 一个 PLS session 内 >9 batch 后触发 TRANSLATION_FAULT
2. **重激活 bug**: 当 PLS session flush 后重新激活，**2+ 次 draw 即损坏**

Rive 代码注释确认: *"the blob has a PLS re-activation bug where 2+ user draws
in a re-activated session corrupt rendering"*

因此只有 `limit=1`（每 batch 一个独立 session，无重激活）才能避免 fault。
这使得 g13p0/g24p0 的编译器优势完全无法利用。

---

## 九、Blob 变体分析

### 可用的 bifrost-g52 blob

| 文件名 | DDK | PLS | MSAA | Vulkan | 推荐 |
|--------|-----|-----|------|--------|------|
| `libmali-bifrost-g52-g2p0-gbm.so` | g2p0 | ✅ 无限制 | ❌ 异常 | ❌ | **⭐ 最佳 blob** |
| `libmali-bifrost-g52-g13p0-gbm.so` | g13p0 | ⚠️ limit=1 only | ✅ 65 上限 | ❌ | 仅 13 FPS |
| `libmali-bifrost-g52-g24p0-gbm.so` | g24p0 | ⚠️ limit=1 only | ✅ 65 上限 | ❌ | 仅 13 FPS |

### Display backend 变体

`-gbm`、`-wayland-gbm`、`-x11-gbm`、`-dummy-gbm` 不影响 GLES 渲染管线。
DRM/KMS 直接渲染场景使用 `-gbm` 即可。

---

## 十、综合推荐路线

### 最终结论: Panfrost PLS 仍是 RK3566 的最佳高性能选项

经过对所有驱动路径的**穷举测试** (2026-02-22 更新):

| 路径 | 500×500 | 720×720 | 渲染正确 | 可行 | 原因 |
|------|---------|---------|---------|------|------|
| **Panfrost GLES PLS** | **48** | **~30** | **✅** | **✅** | **最佳性能 + 稳定** |
| g2p0 Blob PLS (limit=0) | 40 | 24.6 | ✅ | ✅ | 可用但慢 17-40% |
| g13p0 Blob PLS (limit=0) | 47 | — | ❌ | ❌ | 8+ batch 触发 TRANSLATION_FAULT |
| g13p0 Blob PLS (limit=1) | 13 | — | ✅ | ❌ | PLS cycle 开销太大 |
| ~~Blob MSAA 4x~~ | ~~60~~ | ~~59.5~~ | **❌** | **❌** | **角色不渲染** (见下方分析) |
| Panfrost MSAA | FAIL | FAIL | ❌ | ❌ | Rive MSAA shader 与 Mesa 不兼容 |
| Blob Vulkan | — | — | — | ❌ | 所有 blob 无 Vulkan |

### ⚠️ MSAA 模式渲染失败 (2026-02-22 发现)

之前测试 `RIVE_MSAA_SAMPLES=4` 报告了 60 FPS，但**视觉验证发现渲染不完整**：

- 仅渲染背景渐变和树桩，猫头鹰角色、思维泡泡等所有非背景内容**完全缺失**
- g13p0 和 g2p0 blob MSAA 模式均有此问题 (与 blob 版本无关)
- 有/无离屏 FBO 均有此问题
- MSAA FBO 中 65 个 draw batch 执行完毕、resolve blit 成功 (err=0x0)，但像素值仅为背景色
- 高 FPS 是因为 GPU 实际只渲染了简单背景，**不是真实性能**

**根因**: Rive MSAA 渲染路径 (InterlockMode::msaa) 在 Mali-G52 blob 驱动上的 stencil-based
clipping 或 MSAA resolve 存在兼容性问题，导致角色内容被丢弃。

### 性能排名 (已验证渲染正确)

1. **Panfrost GLES PLS @800MHz: ~48 FPS @500×500** — ⭐ 最佳
2. g2p0 Blob PLS @800MHz: ~40 FPS @500×500 — 备选
3. g13p0/g24p0 Blob PLS limit=1: ~13 FPS — 不可用

### 推荐配置 (Panfrost - 默认驱动，无需切换)

```bash
echo performance | sudo tee /sys/devices/platform/fde60000.gpu/devfreq/fde60000.gpu/governor
./rk3566_player dress-up.riv
```

### 备选配置 (g2p0 Blob PLS)

```bash
echo fde60000.gpu | sudo tee /sys/bus/platform/drivers/panfrost/unbind
sudo insmod /lib/modules/6.1.0-1025-rockchip/extra/kbase_mali_jm.ko
echo fde60000.gpu | sudo tee /sys/bus/platform/drivers/mali_jm/bind
sudo ln -sf /dev/mali_jm0 /dev/mali0
echo performance | sudo tee /sys/devices/platform/fde60000.gpu/devfreq/fde60000.gpu/governor
sudo LD_LIBRARY_PATH=/opt/libmali LD_PRELOAD=/opt/libmali/libmali-bifrost-g52-g2p0-gbm.so \
    RIVE_MALI_PLS_PASS_LIMIT=0 \
    ./rk3566_player dress-up.riv
```

---

## 十一、系统设置参考

### 组件版本

| 组件 | 版本 | UK 版本 | 状态 |
|------|------|--------|------|
| **kbase (g25p0, 手动编译)** | g25p0-00eac0 | UK 11.47 | ⭐ 推荐 |
| kbase (Ubuntu 原装) | g18p0-01eac0 | UK 11.38 | 有左眼 bug |
| Blob (g13p0) | g13p0-01eac0 | UK 11.32 | PLS fault; MSAA 不渲染角色 |
| **Blob (g2p0)** | g2p0-01eac0 | — | **⭐ 推荐 (PLS 模式)** |
| Blob (g24p0) | g24p0-00eac0 | UK ~11.40+ | PLS tile 限制 |
| Kernel | 6.1.0-1025-rockchip | — | Ubuntu |

### 设备路径

- kbase 模块: `/lib/modules/6.1.0-1025-rockchip/extra/kbase_mali_jm.ko`
- Blob 库: `/opt/libmali/libmali-bifrost-g52-{g2p0,g13p0,g24p0}-gbm.so`
- 设备节点: `/dev/mali0` → `/dev/mali_jm0`

### 已知系统级问题

1. **设备节点名**: blob 需要 `/dev/mali0`，kbase 创建 `/dev/mali_jm0`，需 symlink
2. **Panfrost 重绑**: kbase unbind 后 Panfrost 无法直接 rebind (OPP regulator EBUSY)，需完全重启
3. **设备启动较慢**: DT 修改后首次启动 SSH 可能需要 3-5 分钟才能连接
4. ~~**GPU 频率 (DT OPP)**~~: ✅ 已修复 — `rockchip,supported-hw` 删除 + `clock-names` 修改
