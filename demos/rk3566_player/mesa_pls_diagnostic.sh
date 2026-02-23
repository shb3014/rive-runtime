#!/bin/bash
# =============================================================================
# Mesa/Panfrost PLS Shader 诊断脚本
# 方案 E: 检查 PLS shader codegen 质量 + 测试 nocrc/noafbc/nofp16
# =============================================================================
# 使用方法: sudo ./mesa_pls_diagnostic.sh <path-to-riv-file> [duration_seconds]
# 示例:     sudo ./mesa_pls_diagnostic.sh ../../dress-up.riv 10
# =============================================================================

set -euo pipefail

PLAYER="./bin/release/rk3566_player"
RIV_FILE="${1:-../../dress-up.riv}"
DURATION="${2:-8}"
RESULTS_DIR="mesa_diag_results"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
RESULTS_DIR="${RESULTS_DIR}/${TIMESTAMP}"

# ---- Colors for output ----
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

info()  { echo -e "${CYAN}[INFO]${NC}  $*"; }
ok()    { echo -e "${GREEN}[OK]${NC}    $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC}  $*"; }
fail()  { echo -e "${RED}[FAIL]${NC}  $*"; }

# ---- Preflight checks ----
if [ ! -f "$PLAYER" ]; then
    fail "Player not found at $PLAYER. Build first with build.sh."
    exit 1
fi

if [ ! -f "$RIV_FILE" ]; then
    fail "Riv file not found: $RIV_FILE"
    exit 1
fi

mkdir -p "$RESULTS_DIR"
SUMMARY="${RESULTS_DIR}/summary.txt"

echo "=============================================" | tee "$SUMMARY"
echo " Mesa/Panfrost PLS Shader 诊断报告" | tee -a "$SUMMARY"
echo " 日期: $(date)" | tee -a "$SUMMARY"
echo " 测试文件: $RIV_FILE" | tee -a "$SUMMARY"
echo " 每测试运行时间: ${DURATION}s" | tee -a "$SUMMARY"
echo "=============================================" | tee -a "$SUMMARY"

# ---- Helper: Run player with env vars and capture output ----
run_test() {
    local test_name="$1"
    shift
    local env_vars="$*"
    local log_file="${RESULTS_DIR}/${test_name}.log"

    echo "" | tee -a "$SUMMARY"
    info "===== 测试: ${test_name} =====" | tee -a "$SUMMARY"
    info "环境变量: ${env_vars:-<none>}" | tee -a "$SUMMARY"

    # Run with timeout, capture both stdout and stderr
    if timeout "${DURATION}s" env $env_vars "$PLAYER" "$RIV_FILE" \
            > "$log_file" 2>&1; then
        ok "正常退出" | tee -a "$SUMMARY"
    else
        local rc=$?
        if [ $rc -eq 124 ]; then
            ok "超时退出 (正常, 采集完成)" | tee -a "$SUMMARY"
        else
            warn "退出码=$rc" | tee -a "$SUMMARY"
        fi
    fi

    # Extract FPS if present
    if grep -q "FPS:" "$log_file" 2>/dev/null; then
        local fps_lines
        fps_lines=$(grep "FPS:" "$log_file" | tail -5)
        echo "  FPS 数据 (最后5条):" | tee -a "$SUMMARY"
        echo "$fps_lines" | sed 's/^/    /' | tee -a "$SUMMARY"

        # Calculate average FPS from all readings
        local avg_fps
        avg_fps=$(grep -oP 'FPS:\s*[\d.]+' "$log_file" \
            | grep -oP '[\d.]+' \
            | awk '{ sum += $1; n++ } END { if(n>0) printf "%.1f", sum/n; else print "N/A" }')
        echo "  平均 FPS: ${avg_fps}" | tee -a "$SUMMARY"
    else
        warn "  无 FPS 数据" | tee -a "$SUMMARY"
    fi

    echo "  日志: $log_file" | tee -a "$SUMMARY"
}

# ===========================================================================
# 测试 1: 基线 (无额外 debug 标志)
# ===========================================================================
run_test "01_baseline" \
    RIVE_RENDER_WIDTH=500 RIVE_RENDER_HEIGHT=500

# ===========================================================================
# 测试 2: BIFROST_MESA_DEBUG=shaders — 输出编译后的 Bifrost IR
# 这是最核心的诊断: 检查 PLS shader 是否正确编译
# ===========================================================================
info ""
info "===== 测试 2: Shader Codegen 检查 (BIFROST_MESA_DEBUG=shaders) =====" | tee -a "$SUMMARY"
info "  注意: 这会产生大量输出, 仅运行3秒" | tee -a "$SUMMARY"

SHADER_LOG="${RESULTS_DIR}/02_shaders.log"
SHADER_DURATION=3

if timeout "${SHADER_DURATION}s" env \
    BIFROST_MESA_DEBUG=shaders \
    RIVE_RENDER_WIDTH=500 RIVE_RENDER_HEIGHT=500 \
    "$PLAYER" "$RIV_FILE" > "$SHADER_LOG" 2>&1; then
    :
else
    :
fi

# Analyze shader output
echo "  Shader 日志: $SHADER_LOG" | tee -a "$SUMMARY"
if [ -s "$SHADER_LOG" ]; then
    SHADER_SIZE=$(wc -c < "$SHADER_LOG")
    echo "  日志大小: ${SHADER_SIZE} bytes" | tee -a "$SUMMARY"

    # Check for PLS-related instructions
    echo "" | tee -a "$SUMMARY"
    echo "  === Shader Codegen 分析 ===" | tee -a "$SUMMARY"

    # Check for load_tile / store_tile (PLS backing operations)
    LOAD_TILE_COUNT=$(grep -ci 'load_tile\|ld_tile\|LOAD.*tile' "$SHADER_LOG" 2>/dev/null || echo 0)
    STORE_TILE_COUNT=$(grep -ci 'store_tile\|st_tile\|STORE.*tile' "$SHADER_LOG" 2>/dev/null || echo 0)
    echo "  load_tile 指令数: $LOAD_TILE_COUNT" | tee -a "$SUMMARY"
    echo "  store_tile 指令数: $STORE_TILE_COUNT" | tee -a "$SUMMARY"

    # Check for PLS_PRESERVE becoming no-op
    PRESERVE_COUNT=$(grep -ci 'preserve\|PLS_PRESERVE\|nop.*pls' "$SHADER_LOG" 2>/dev/null || echo 0)
    echo "  PLS_PRESERVE 相关: $PRESERVE_COUNT" | tee -a "$SUMMARY"

    # Check for FP16 usage
    FP16_COUNT=$(grep -ci 'f16\|half\|fp16\|h[0-9]' "$SHADER_LOG" 2>/dev/null || echo 0)
    FP32_COUNT=$(grep -ci 'f32\|float\|fp32\|r[0-9]' "$SHADER_LOG" 2>/dev/null || echo 0)
    echo "  FP16 指令/引用: $FP16_COUNT" | tee -a "$SUMMARY"
    echo "  FP32 指令/引用: $FP32_COUNT" | tee -a "$SUMMARY"
    if [ "$FP16_COUNT" -gt 0 ] && [ "$FP32_COUNT" -gt 0 ]; then
        FP16_RATIO=$(awk "BEGIN { printf \"%.1f\", 100.0 * $FP16_COUNT / ($FP16_COUNT + $FP32_COUNT) }")
        echo "  FP16 占比: ${FP16_RATIO}%" | tee -a "$SUMMARY"
    fi

    # Count total shader compilations
    SHADER_COMPILE_COUNT=$(grep -ci 'shader\|compil' "$SHADER_LOG" 2>/dev/null || echo 0)
    echo "  Shader 编译相关行: $SHADER_COMPILE_COUNT" | tee -a "$SUMMARY"

    # Extract fragment shader sections (they're the most interesting)
    FRAG_SHADER_FILE="${RESULTS_DIR}/02_fragment_shaders_only.log"
    grep -A 50 -i 'fragment\|frag.*shader' "$SHADER_LOG" > "$FRAG_SHADER_FILE" 2>/dev/null || true
    FRAG_SIZE=$(wc -c < "$FRAG_SHADER_FILE" 2>/dev/null || echo 0)
    echo "  Fragment shader 摘录: $FRAG_SHADER_FILE (${FRAG_SIZE} bytes)" | tee -a "$SUMMARY"
else
    warn "  Shader 日志为空 — BIFROST_MESA_DEBUG 可能不被此 Mesa 构建支持" | tee -a "$SUMMARY"
fi

# ===========================================================================
# 测试 3: BIFROST_MESA_DEBUG=shaderdb — 统计信息模式
# ===========================================================================
info ""
info "===== 测试 3: ShaderDB 统计 (BIFROST_MESA_DEBUG=shaderdb) =====" | tee -a "$SUMMARY"

SHADERDB_LOG="${RESULTS_DIR}/03_shaderdb.log"
if timeout "${SHADER_DURATION}s" env \
    BIFROST_MESA_DEBUG=shaderdb \
    RIVE_RENDER_WIDTH=500 RIVE_RENDER_HEIGHT=500 \
    "$PLAYER" "$RIV_FILE" > "$SHADERDB_LOG" 2>&1; then
    :
else
    :
fi

echo "  ShaderDB 日志: $SHADERDB_LOG" | tee -a "$SUMMARY"
if [ -s "$SHADERDB_LOG" ]; then
    # shaderdb output typically looks like:
    # SHADER-DB: <name> - <type>: <instr> instructions, <bundles> bundles, ...
    SHADERDB_ENTRIES=$(grep -ci 'SHADER-DB\|shader-db\|shader_db' "$SHADERDB_LOG" 2>/dev/null || echo 0)
    echo "  ShaderDB 条目数: $SHADERDB_ENTRIES" | tee -a "$SUMMARY"

    if [ "$SHADERDB_ENTRIES" -gt 0 ]; then
        echo "  ShaderDB 摘要 (前20条):" | tee -a "$SUMMARY"
        grep -i 'SHADER-DB\|shader-db\|shader_db' "$SHADERDB_LOG" | head -20 | sed 's/^/    /' | tee -a "$SUMMARY"
    fi
else
    warn "  ShaderDB 日志为空" | tee -a "$SUMMARY"
fi

# ===========================================================================
# 测试 4: BIFROST_MESA_DEBUG=shaders,shaderdb — 组合模式
# ===========================================================================
info ""
info "===== 测试 4: Shaders+ShaderDB 组合 =====" | tee -a "$SUMMARY"

COMBINED_LOG="${RESULTS_DIR}/04_shaders_shaderdb.log"
if timeout "${SHADER_DURATION}s" env \
    BIFROST_MESA_DEBUG=shaders,shaderdb \
    RIVE_RENDER_WIDTH=500 RIVE_RENDER_HEIGHT=500 \
    "$PLAYER" "$RIV_FILE" > "$COMBINED_LOG" 2>&1; then
    :
else
    :
fi

COMBINED_SIZE=$(wc -c < "$COMBINED_LOG" 2>/dev/null || echo 0)
echo "  组合日志: $COMBINED_LOG (${COMBINED_SIZE} bytes)" | tee -a "$SUMMARY"

# ===========================================================================
# 测试 5: PAN_MESA_DEBUG=nocrc — 禁用 Transaction Elimination
# ===========================================================================
run_test "05_nocrc" \
    PAN_MESA_DEBUG=nocrc \
    RIVE_RENDER_WIDTH=500 RIVE_RENDER_HEIGHT=500

# ===========================================================================
# 测试 6: PAN_MESA_DEBUG=noafbc — 禁用 AFBC (ARM Frame Buffer Compression)
# ===========================================================================
run_test "06_noafbc" \
    PAN_MESA_DEBUG=noafbc \
    RIVE_RENDER_WIDTH=500 RIVE_RENDER_HEIGHT=500

# ===========================================================================
# 测试 7: PAN_MESA_DEBUG=nofp16 — 禁用 FP16 优化
# ===========================================================================
run_test "07_nofp16" \
    PAN_MESA_DEBUG=nofp16 \
    RIVE_RENDER_WIDTH=500 RIVE_RENDER_HEIGHT=500

# ===========================================================================
# 测试 8: PAN_MESA_DEBUG=perf — 启用性能计数器输出
# ===========================================================================
PERF_LOG="${RESULTS_DIR}/08_perf.log"
info ""
info "===== 测试 8: 性能计数器 (PAN_MESA_DEBUG=perf) =====" | tee -a "$SUMMARY"

if timeout "${DURATION}s" env \
    PAN_MESA_DEBUG=perf \
    RIVE_RENDER_WIDTH=500 RIVE_RENDER_HEIGHT=500 \
    "$PLAYER" "$RIV_FILE" > "$PERF_LOG" 2>&1; then
    :
else
    :
fi

echo "  性能日志: $PERF_LOG" | tee -a "$SUMMARY"
if [ -s "$PERF_LOG" ]; then
    TE_COUNT=$(grep -ci 'transaction.*elim\|TE\|crc' "$PERF_LOG" 2>/dev/null || echo 0)
    echo "  Transaction Elimination 相关: $TE_COUNT" | tee -a "$SUMMARY"
    if [ "$TE_COUNT" -gt 0 ]; then
        grep -i 'transaction.*elim\|TE\|crc' "$PERF_LOG" | head -10 | sed 's/^/    /' | tee -a "$SUMMARY"
    fi
fi

# ===========================================================================
# 测试 9: 组合禁用 — nocrc + noafbc
# ===========================================================================
run_test "09_nocrc_noafbc" \
    PAN_MESA_DEBUG=nocrc,noafbc \
    RIVE_RENDER_WIDTH=500 RIVE_RENDER_HEIGHT=500

# ===========================================================================
# 测试 10: 组合禁用 — nocrc + nofp16
# ===========================================================================
run_test "10_nocrc_nofp16" \
    PAN_MESA_DEBUG=nocrc,nofp16 \
    RIVE_RENDER_WIDTH=500 RIVE_RENDER_HEIGHT=500

# ===========================================================================
# 测试 11: 全部禁用 — nocrc + noafbc + nofp16
# ===========================================================================
run_test "11_all_disabled" \
    PAN_MESA_DEBUG=nocrc,noafbc,nofp16 \
    RIVE_RENDER_WIDTH=500 RIVE_RENDER_HEIGHT=500

# ===========================================================================
# 测试 12: 384x384 基线 (已知能达到 60 FPS)
# ===========================================================================
run_test "12_384_baseline" \
    RIVE_RENDER_WIDTH=384 RIVE_RENDER_HEIGHT=384

# ===========================================================================
# GPU 硬件状态
# ===========================================================================
echo "" | tee -a "$SUMMARY"
echo "=============================================" | tee -a "$SUMMARY"
echo " GPU 硬件信息" | tee -a "$SUMMARY"
echo "=============================================" | tee -a "$SUMMARY"

if [ -f /sys/class/devfreq/fde60000.gpu/cur_freq ]; then
    echo "  GPU 当前频率: $(cat /sys/class/devfreq/fde60000.gpu/cur_freq) Hz" | tee -a "$SUMMARY"
fi
if [ -f /sys/class/devfreq/fde60000.gpu/max_freq ]; then
    echo "  GPU 最大频率: $(cat /sys/class/devfreq/fde60000.gpu/max_freq) Hz" | tee -a "$SUMMARY"
fi
if [ -f /sys/class/devfreq/fde60000.gpu/governor ]; then
    echo "  GPU 调频策略: $(cat /sys/class/devfreq/fde60000.gpu/governor)" | tee -a "$SUMMARY"
fi

# Kernel DRM driver version
if [ -d /sys/class/drm/card0 ]; then
    echo "  DRM card: $(cat /sys/class/drm/card0/device/uevent 2>/dev/null | head -3 | tr '\n' ' ')" | tee -a "$SUMMARY"
fi

# Mesa version
MESA_VERSION=$(strings "$PLAYER" 2>/dev/null | grep -i "mesa\s*2" | head -1 || echo "unknown")
echo "  Mesa version (from binary): $MESA_VERSION" | tee -a "$SUMMARY"

echo "" | tee -a "$SUMMARY"
echo "=============================================" | tee -a "$SUMMARY"
echo " 诊断完成" | tee -a "$SUMMARY"
echo " 结果目录: $RESULTS_DIR" | tee -a "$SUMMARY"
echo " 摘要文件: $SUMMARY" | tee -a "$SUMMARY"
echo "=============================================" | tee -a "$SUMMARY"

# ===========================================================================
# FPS 对比表
# ===========================================================================
echo "" | tee -a "$SUMMARY"
echo "===== FPS 对比汇总 =====" | tee -a "$SUMMARY"
printf "%-30s %10s\n" "测试配置" "平均FPS" | tee -a "$SUMMARY"
printf "%-30s %10s\n" "------------------------------" "----------" | tee -a "$SUMMARY"

for logfile in "${RESULTS_DIR}"/*.log; do
    test_name=$(basename "$logfile" .log)
    avg_fps=$(grep -oP 'FPS:\s*[\d.]+' "$logfile" 2>/dev/null \
        | grep -oP '[\d.]+' \
        | awk '{ sum += $1; n++ } END { if(n>0) printf "%.1f", sum/n; else print "N/A" }')
    printf "%-30s %10s\n" "$test_name" "$avg_fps" | tee -a "$SUMMARY"
done

echo ""
ok "全部诊断完成! 请查看 $SUMMARY 获取完整报告。"
echo ""
echo "下一步建议:"
echo "  1. 检查 02_shaders.log 中 PLS fragment shader 的 Bifrost IR 指令质量"
echo "  2. 比较 05_nocrc vs 01_baseline, 如果 nocrc 更快则 TE 有负面影响"
echo "  3. 比较 06_noafbc vs 01_baseline, 如果 noafbc 更快则 AFBC 解压是瓶颈"
echo "  4. 比较 07_nofp16 vs 01_baseline, 如果更慢则确认 FP16 正常工作"
echo "  5. 检查 03_shaderdb.log 中的指令计数, 寻找异常高的 shader"
