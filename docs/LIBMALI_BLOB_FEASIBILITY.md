# 方案 G: ARM Mali Blob 驱动 (libmali) 可行性分析

> 调查日期: 2026-02-14
> 来源: https://github.com/tsukumijima/libmali-rockchip
> 目标: 评估 ARM 闭源 Mali 驱动能否提升 Rive 在 RK3566 / Mali-G52 上的渲染性能

---

## 一、背景：两套驱动栈

Mali-G52 有两条完全独立的驱动路径：

| 组件 | 开源栈 (当前使用) | 闭源栈 (blob) |
|------|-------------------|---------------|
| **内核模块 (KMD)** | `panfrost.ko` (DRM 驱动) | `mali_kbase` (ARM DDK, built-in `=y`) |
| **用户空间 (UMD)** | Mesa Panfrost (`panfrost_dri.so`) | ARM `libmali-bifrost-g52-*.so` |
| **Shader 编译器** | Mesa NIR → Bifrost ISA (开源) | ARM 专有编译器 (高度优化) |
| **PLS 实现** | Mesa 26.0.0-devel 新加的实验性支持 | **ARM 原生实现** (PLS 就是 ARM 为 Mali 设计的) |
| **OpenGL ES** | 3.1 | **3.2** (完整支持) |
| **Vulkan** | PanVK (1.0, 不完整) | 可能支持 (需确认版本) |

**核心论点**: `GL_EXT_shader_pixel_local_storage` 是 ARM 专门为 Mali 瓦片架构设计的扩展。blob 驱动是 PLS 的**参考实现**，直接映射到硬件 tile buffer。而 Panfrost/Mesa 是社区逆向工程的实现，效率天然低于原厂驱动。

---

## 二、设备现状分析 (2026-02-14 实测)

### 2.1 内核中已有 kbase 驱动 (但探测失败)

```
内核版本: 6.1.0-1025-rockchip
kbase DDK: g18p0-01eac0 (UK version 1.18)
```

**dmesg 关键日志**:
```
[16.023853] mali fde60000.gpu: Kernel DDK version g18p0-01eac0
[16.025507] mali fde60000.gpu: error -ENXIO: IRQ JOB not found
[16.027123] mali fde60000.gpu: error -ENXIO: IRQ MMU not found
[16.028785] mali fde60000.gpu: error -ENXIO: IRQ GPU not found
[16.030381] mali fde60000.gpu: Insufficient register space, will override to the required size
[16.033272] mali fde60000.gpu: Register window unavailable
[16.036220] mali fde60000.gpu: Failed to map registers
[16.039105] mali fde60000.gpu: Register map failed error = -5
[16.042056] mali fde60000.gpu: Device initialization failed
[16.045042] mali: probe of fde60000.gpu failed with error -5

[25.826846] panfrost fde60000.gpu: clock rate = 594000000  ← panfrost 随后成功接管
[25.905944] panfrost fde60000.gpu: mali-g52 id 0x7402
```

### 2.2 失败原因分析

| 问题 | 设备树现状 | kbase 期望 |
|------|-----------|-----------|
| **中断名称** | `interrupt-names = "gpu", "mmu", "job"` (小写) | `"GPU"`, `"MMU"`, `"JOB"` (大写) |
| **寄存器空间** | `reg = <0 0xfde60000 0 0x4000>` (16KB) | 通常需要 0x40000 (256KB) |

`platform_get_irq_byname()` 是**区分大小写**的。主线内核 DT 使用小写 (Panfrost 兼容)，但 ARM kbase 期望大写。这是 kbase 探测失败的直接原因。

### 2.3 当前 GPU 绑定状态

```
/sys/bus/platform/drivers/panfrost/fde60000.gpu  → 已绑定 panfrost
/sys/bus/platform/drivers/mali/                   → 已注册但无设备绑定
/dev/mali*                                        → 不存在 (kbase 探测失败)
```

---

## 三、libmali-rockchip 仓库分析

### 3.1 可用 blob 版本 (Mali-G52, aarch64)

| Blob 文件 | DDK 版本 | 后缀选项 |
|-----------|---------|---------|
| `libmali-bifrost-g52-g2p0-*.so` | g2p0 (早期) | gbm, dummy, x11, wayland |
| `libmali-bifrost-g52-g13p0-*.so` | g13p0 | gbm, dummy, x11, wayland, x11-wayland |
| `libmali-bifrost-g52-g24p0-*.so` | **g24p0** (最新) | gbm, dummy, x11, wayland, x11-wayland |

对我们的 DRM/KMS 无桌面环境: 使用 **`libmali-bifrost-g52-g24p0-gbm.so`**

### 3.2 版本兼容性问题 (关键风险)

```
内核 kbase DDK:  g18p0
最佳 blob DDK:   g24p0
```

ARM 的 kbase 内核模块和 libmali 用户空间通过私有 IOCTL 接口通信。**DDK 版本不匹配可能导致 IOCTL 不兼容**。

| 方案 | 复杂度 | 风险 |
|------|--------|------|
| 直接用 g24p0 blob + g18p0 kbase | 低 | **高** — IOCTL 可能不兼容 |
| 从 ARM 源码构建 g24p0 kbase 模块 | 中 | 中 — 需要编译环境 |
| 降级用 g13p0 blob + g18p0 kbase | 低 | 中 — g13p0 较接近 g18p0 |
| 使用 Rockchip BSP 内核 (含匹配 kbase) | 高 | 低 — 但可能丢失主线功能 |

### 3.3 Vulkan 支持

blob 包含 Vulkan ICD (`vk_icdGetInstanceProcAddr`)，且 `meson.build` 中配置了 `MaliVulkan.so`。Mali-G52 blob 通常支持 **Vulkan 1.1**，这将直接解决我们之前遇到的 PanVK Vulkan 1.0 限制。

---

## 四、预期性能提升分析

### 4.1 为什么 blob 驱动会更快

| 因素 | Panfrost (当前) | Blob (预期) | 影响 |
|------|----------------|-------------|------|
| **PLS 实现** | Mesa 实验性实现，间接映射 | ARM 原生硬件映射 | **大** — PLS 是主要瓶颈 |
| **Shader 编译质量** | NIR → Bifrost (社区实现) | ARM 专有编译器 | **大** — 寄存器分配/调度差异 |
| **AFBC/TE 优化** | 基础支持 | 深度集成 | 中 — 减少带宽 |
| **FP16 优化** | 可能有 bug (nofp16 反而更快) | 成熟的 FP16 通道 | 中 |
| **OpenGL ES 3.2** | 3.1 限制 | 3.2 完整 | 小 — Rive 主要用 3.0/3.1 |
| **GL_EXT_shader_pixel_local_storage2** | 可能不支持 | 几乎确定支持 | 中 — 允许运行时调整 PLS 大小 |

### 4.2 预估性能范围

基于 Mali 社区中 Panfrost vs blob 的已知性能差距（对于复杂 shader workload）：

| 场景 | 保守估计 | 乐观估计 |
|------|---------|---------|
| PLS 密集渲染 (dress-up.riv) | **+30-50%** | **+80-120%** |
| 简单内容 | +10-20% | +30-50% |

**如果乐观估计成立**: 500×500 从 ~45 FPS → 60+ FPS，无需降分辨率。

### 4.3 类比论证

在我们的测试中，`nofp16` 就改善了 3% — 这只是 Mesa shader 编译的一个微小调整。ARM 的专有编译器拥有完整的 Bifrost ISA 知识和多年优化经验，对于 PLS 密集的 4-plane shader，整体编译质量差距可能远大于 3%。

---

## 五、实施路径

### 阶段 1: 修复 DT 让 kbase 探测成功 (1-2 小时)

```bash
# 1. 创建 DT overlay 修复中断名称
# 修改 interrupt-names: "gpu" → "GPU", "mmu" → "MMU", "job" → "JOB"
# 可能需要扩大 reg 空间: 0x4000 → 0x40000

# 2. 黑名单 panfrost 模块
echo "blacklist panfrost" | sudo tee /etc/modprobe.d/blacklist-panfrost.conf

# 3. 应用 overlay 并重启
# 4. 验证 /dev/mali0 出现
```

**验证标准**: 重启后 `dmesg | grep mali` 无错误，`/dev/mali0` 存在。

### 阶段 2: 安装 blob 并测试 GL 扩展 (30 分钟)

```bash
# 1. 下载 blob
wget https://github.com/tsukumijima/libmali-rockchip/raw/master/lib/aarch64-linux-gnu/libmali-bifrost-g52-g24p0-gbm.so

# 2. 安装为 /usr/lib/aarch64-linux-gnu/libmali.so
sudo cp libmali-bifrost-g52-g24p0-gbm.so /usr/lib/aarch64-linux-gnu/libmali.so
sudo ldconfig

# 3. 创建 GL/EGL 符号链接
sudo ln -sf libmali.so /usr/lib/aarch64-linux-gnu/libEGL.so.1
sudo ln -sf libmali.so /usr/lib/aarch64-linux-gnu/libGLESv2.so.2
sudo ln -sf libmali.so /usr/lib/aarch64-linux-gnu/libgbm.so.1

# 4. 验证 GL 扩展
# 检查 GL_EXT_shader_pixel_local_storage 是否在扩展列表中
```

### 阶段 3: 链接 Rive Player 到 blob (1 小时)

```bash
# 方案 A: 修改 LD_LIBRARY_PATH 指向 blob
export LD_LIBRARY_PATH=/usr/lib/aarch64-linux-gnu  # blob 路径

# 方案 B: 重新编译 Rive Player 链接到 blob 的 GLES/EGL
# (如果 blob 的 EGL 接口与 Mesa 有差异)

# 运行测试
./rive_player --source dress-up.riv --width 500 --height 500
```

### 阶段 4: 性能对比 (30 分钟)

```bash
# 对比矩阵:
# - Panfrost + Mesa PLS:  基线 (~45 FPS @500x500)
# - Blob + 原生 PLS:     新测量
# - 不同分辨率: 384, 500, 600, 700, 800
```

---

## 六、风险评估

| 风险 | 严重度 | 可能性 | 缓解措施 |
|------|--------|--------|----------|
| DDK 版本不匹配 (g18p0 kbase vs g24p0 blob) | **高** | **高** | 尝试 g13p0 blob；或从 ARM 源码构建 g24p0 kbase |
| DT overlay 不完整/出错 | 中 | 中 | 准备恢复方案 (去掉 overlay + 取消 panfrost 黑名单) |
| blob 的 GBM 实现与 Rive 的 DRM/KMS 代码不兼容 | 中 | 低 | blob 的 GBM 接口是标准的 |
| blob 不支持我们的 GLES 扩展用法 | 低 | 低 | blob 是 PLS 的参考实现 |
| 闭源驱动 bug 无法调试 | 中 | 中 | 保留 Panfrost 恢复路径 |
| blob 许可证限制 | 低 | 低 | ARM EULA 允许产品使用 |

---

## 七、与现有方案的优先级对比

| 方案 | 预期提升 | 工作量 | 风险 | 推荐优先级 |
|------|---------|--------|------|-----------|
| **G: libmali blob** | **+30-100%** | **中 (3-5h)** | **中-高** | **⭐⭐⭐⭐⭐ 最高** |
| A: 动态分辨率 | +60% (降分辨率换帧率) | 低 (2-3h) | 低 | ⭐⭐⭐⭐ |
| D: PLS 平面削减 | +10-25% | 高 (1-2 周) | 中 | ⭐⭐⭐ |
| F: 静态层缓存 | +50-200% (场景相关) | 高 (2-4 周) | 中 | ⭐⭐⭐ |
| nofp16 调查 | +3% | 低 (1h) | 低 | ⭐⭐ |

**结论**: libmali blob 驱动是**投入产出比最高**的方案。如果成功，可能是唯一能让 dress-up.riv 在原生 500×500 分辨率达到 60 FPS 的路径，且不需要修改 Rive 代码。

---

## 八、快速验证步骤 (建议首先做)

在投入完整实施前，先做一个低风险快速验证：

```bash
# 在设备上快速检查 blob 是否能加载 (不需要修 kbase)
# 下载 blob 并检查其依赖
readelf -d libmali-bifrost-g52-g24p0-gbm.so | grep NEEDED
# 如果只依赖 libc/libdl/libpthread 等标准库，说明 blob 是自包含的

# 检查 blob 导出的 GL 函数
nm -D libmali-bifrost-g52-g24p0-gbm.so | grep -i "pixel_local"
# 如果找到 glFramebufferPixelLocalStorageSizeEXT 等，确认 PLS 支持
```

---

## 九、回退策略

如果 blob 方案失败，可以无损回退：

```bash
# 1. 移除 panfrost 黑名单
sudo rm /etc/modprobe.d/blacklist-panfrost.conf

# 2. 移除 DT overlay
# 3. 重启 → panfrost 自动恢复

# 4. 继续使用 Mesa PLS 路径
export LD_LIBRARY_PATH=/opt/mesa-pls-vk/lib/aarch64-linux-gnu
export LIBGL_DRIVERS_PATH=/opt/mesa-pls-vk/lib/aarch64-linux-gnu/dri
```

所有修改均为可逆的，不影响现有开源栈。
