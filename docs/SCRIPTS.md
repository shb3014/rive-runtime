# 脚本索引（`scripts/`）

本仓库把“为 Orange Pi 3B / RK3566 调试与跑分”新增的脚本集中在顶层目录 `scripts/`，便于统一管理与引用。

> 说明：很多脚本假设你在 **目标板（Orange Pi 3B）** 上运行，或需要你能通过 SSH 访问目标板。

---

## 常用环境（目标板）

多数脚本/运行方式都需要使用自定义 Mesa（如 `/opt/mesa-pls`）：

```bash
export LD_LIBRARY_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu:$LD_LIBRARY_PATH
export LIBGL_DRIVERS_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu/dri
```

---

## 脚本列表

### 运行/入口

- `scripts/run_player_with_pls.sh`
  - **用途**：设置 Mesa 运行时环境变量后直接启动 `rk3566_player`
  - **典型用法**：`scripts/run_player_with_pls.sh ~/dress-up.riv`

### 性能/跑分

- `scripts/test_fps_deep_investigation.sh`
  - **用途**：深度 FPS 测试（多阶段：vsync、分辨率甜点区、interlock 变量等），输出结果文件
  - **运行位置**：通常在目标板的 `~/rive-runtime/demos/rk3566_player/` 下运行（脚本里假设 `bin/release/rk3566_player` 相对路径存在）

- `scripts/test_fps_comprehensive.sh`
  - **用途**：更“矩阵式”的配置扫描（分辨率、PLS/Interlock、VSync、Mesa 环境变量、stretch 等）
  - **注意**：脚本会直接调用 `timeout/grep/awk` 做简单统计

- `scripts/test_renderer_modes_500x500.sh`
  - **用途**：固定 500×500 下测试 renderer mode 相关开关（FSI/RO/MSAA/clockwise/stretch/vsync）

- `scripts/QUICK_PERFORMANCE_BOOST.sh`
  - **用途**：把 CPU/GPU governor 设为 performance（需要 `sudo`）
  - **风险**：可能增加功耗与温度；是否有效取决于内核/设备树/权限

### 正确性对比/可视化验证

- `scripts/test_pls_vs_fallback.sh`
  - **用途**：交互式对比“PLS 默认路径”和“禁用 PLS（MSAA fallback）”的视觉差异

- `scripts/test_path_rendering.sh`
  - **用途**：交互式检查路径/椭圆渲染问题是否修复

### 调试/诊断/抓 trace

- `scripts/comprehensive_diagnostic.sh`
  - **用途**：一键跑一组诊断测试（不同开关组合 + 扩展信息输出），帮助定位是否 PLS 特定问题

- `scripts/setup_apitrace.sh`
  - **用途**：在目标板安装 apitrace 并抓取短 trace（示例输出到 `~/ellipse_bug.trace`）

### 部署/打补丁辅助

- `scripts/deploy_changes.sh`
  - **用途**：把本机修改的源码复制到目标板并触发重编译
  - **注意**：依赖可用的 SSH 登录方式（建议 SSH key）

- `scripts/apply_mesa_debug.sh`
  - **用途**：在目标板上对 Mesa 源码做一些“注入式”调试修改并重编译/重新安装
  - **注意**：依赖可用的 SSH 登录方式（建议 SSH key）；脚本包含多处 `sed -i` 修改，适合临时实验，不建议作为长期工作流

- `scripts/apply_pls_fix.sh`
  - **用途**：提示你如何手动应用一次 PLS 相关修复（包含备份/回滚提示）

---

## 使用建议

- **跑分脚本**：优先使用 `scripts/test_fps_deep_investigation.sh` 或 `scripts/test_fps_comprehensive.sh`，并把结果与使用的 Mesa/Rive 版本一起记录。
- **正确性优先**：涉及 `RIVE_DISABLE_PLS` / `RIVE_MSAA_SAMPLES` / `RIVE_CLOCKWISE` 这类开关时，先做视觉验证。
- **部署与调试脚本**：建议把目标板信息（IP/用户名等）参数化，避免在脚本中硬编码敏感信息。

