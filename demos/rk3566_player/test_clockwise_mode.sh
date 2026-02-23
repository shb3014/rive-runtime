#!/bin/bash
# =============================================================================
# Clockwise 模式测试脚本
# 方案 B: 用 RIVE_CLOCKWISE=1 测试 dress-up.riv, 验证视觉正确性和 FPS 变化
# =============================================================================
# 使用方法: sudo ./test_clockwise_mode.sh [riv_file] [duration]
# 示例:     sudo ./test_clockwise_mode.sh ../../dress-up.riv 15
#
# 此脚本执行以下对比测试:
#   1. 基线 (标准 raster-order, RIVE_CLOCKWISE=0)
#   2. Clockwise 模式 (RIVE_CLOCKWISE=1)
#   3. 在 384x384 和 500x500 分辨率下分别测试
#   4. 输出 FPS 对比表 + 视觉正确性检查指南
# =============================================================================

set -euo pipefail

PLAYER="./bin/release/rk3566_player"
RIV_FILE="${1:-../../dress-up.riv}"
DURATION="${2:-10}"
RESULTS_DIR="clockwise_test_results"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
RESULTS_DIR="${RESULTS_DIR}/${TIMESTAMP}"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

info()  { echo -e "${CYAN}[INFO]${NC}  $*"; }
ok()    { echo -e "${GREEN}[OK]${NC}    $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC}  $*"; }
fail()  { echo -e "${RED}[FAIL]${NC}  $*"; }

if [ ! -f "$PLAYER" ]; then
    fail "Player not found: $PLAYER"
    exit 1
fi

if [ ! -f "$RIV_FILE" ]; then
    fail "Riv file not found: $RIV_FILE"
    exit 1
fi

mkdir -p "$RESULTS_DIR"
SUMMARY="${RESULTS_DIR}/summary.txt"

echo "=============================================" | tee "$SUMMARY"
echo " Clockwise 模式对比测试" | tee -a "$SUMMARY"
echo " 日期: $(date)" | tee -a "$SUMMARY"
echo " 测试文件: $RIV_FILE" | tee -a "$SUMMARY"
echo " 每测试运行: ${DURATION}s" | tee -a "$SUMMARY"
echo "=============================================" | tee -a "$SUMMARY"

# Helper: Run test and extract FPS
run_and_measure() {
    local test_name="$1"
    shift
    local env_vars="$*"
    local log_file="${RESULTS_DIR}/${test_name}.log"

    info "运行: ${test_name}" | tee -a "$SUMMARY"
    info "  环境: ${env_vars}" | tee -a "$SUMMARY"

    if timeout "${DURATION}s" env $env_vars "$PLAYER" "$RIV_FILE" \
            > "$log_file" 2>&1; then
        :
    else
        local rc=$?
        if [ $rc -ne 124 ]; then
            warn "  异常退出码=$rc" | tee -a "$SUMMARY"
        fi
    fi

    # Extract FPS
    local avg_fps="N/A"
    local min_fps="N/A"
    local max_fps="N/A"
    local fps_count=0

    if grep -q "FPS:" "$log_file" 2>/dev/null; then
        avg_fps=$(grep -oP 'FPS:\s*[\d.]+' "$log_file" \
            | grep -oP '[\d.]+' \
            | awk '{ sum += $1; n++ } END { if(n>0) printf "%.1f", sum/n; else print "N/A" }')
        min_fps=$(grep -oP 'FPS:\s*[\d.]+' "$log_file" \
            | grep -oP '[\d.]+' \
            | sort -n | head -1)
        max_fps=$(grep -oP 'FPS:\s*[\d.]+' "$log_file" \
            | grep -oP '[\d.]+' \
            | sort -n | tail -1)
        fps_count=$(grep -c "FPS:" "$log_file" 2>/dev/null || echo 0)
    fi

    echo "  平均FPS=$avg_fps  最小=$min_fps  最大=$max_fps  采样=$fps_count" | tee -a "$SUMMARY"

    # Check for GL errors or crashes
    local gl_errors
    gl_errors=$(grep -ci 'GL_ERROR\|glError\|GL error\|ERROR' "$log_file" 2>/dev/null || echo 0)
    if [ "$gl_errors" -gt 0 ]; then
        warn "  GL错误数: $gl_errors" | tee -a "$SUMMARY"
        grep -i 'GL_ERROR\|glError\|GL error\|ERROR' "$log_file" | head -5 | sed 's/^/    /' | tee -a "$SUMMARY"
    else
        ok "  无 GL 错误" | tee -a "$SUMMARY"
    fi

    # Return FPS as the "result" via a temp file
    echo "$avg_fps" > "${RESULTS_DIR}/.last_fps"
    echo "" | tee -a "$SUMMARY"
}

# ===========================================================================
# 测试组 1: 500x500 分辨率
# ===========================================================================
echo "" | tee -a "$SUMMARY"
echo -e "${BOLD}===== 500x500 分辨率测试 =====${NC}" | tee -a "$SUMMARY"

run_and_measure "500_rasterorder" \
    RIVE_RENDER_WIDTH=500 RIVE_RENDER_HEIGHT=500 RIVE_CLOCKWISE=0
FPS_500_RO=$(cat "${RESULTS_DIR}/.last_fps")

run_and_measure "500_clockwise" \
    RIVE_RENDER_WIDTH=500 RIVE_RENDER_HEIGHT=500 RIVE_CLOCKWISE=1
FPS_500_CW=$(cat "${RESULTS_DIR}/.last_fps")

# ===========================================================================
# 测试组 2: 384x384 分辨率
# ===========================================================================
echo "" | tee -a "$SUMMARY"
echo -e "${BOLD}===== 384x384 分辨率测试 =====${NC}" | tee -a "$SUMMARY"

run_and_measure "384_rasterorder" \
    RIVE_RENDER_WIDTH=384 RIVE_RENDER_HEIGHT=384 RIVE_CLOCKWISE=0
FPS_384_RO=$(cat "${RESULTS_DIR}/.last_fps")

run_and_measure "384_clockwise" \
    RIVE_RENDER_WIDTH=384 RIVE_RENDER_HEIGHT=384 RIVE_CLOCKWISE=1
FPS_384_CW=$(cat "${RESULTS_DIR}/.last_fps")

# ===========================================================================
# 测试组 3: 448x448 分辨率 (中间测试点)
# ===========================================================================
echo "" | tee -a "$SUMMARY"
echo -e "${BOLD}===== 448x448 分辨率测试 =====${NC}" | tee -a "$SUMMARY"

run_and_measure "448_rasterorder" \
    RIVE_RENDER_WIDTH=448 RIVE_RENDER_HEIGHT=448 RIVE_CLOCKWISE=0
FPS_448_RO=$(cat "${RESULTS_DIR}/.last_fps")

run_and_measure "448_clockwise" \
    RIVE_RENDER_WIDTH=448 RIVE_RENDER_HEIGHT=448 RIVE_CLOCKWISE=1
FPS_448_CW=$(cat "${RESULTS_DIR}/.last_fps")

# ===========================================================================
# 测试组 4: Clockwise + 禁用 Raster Ordering (测试极限)
# ===========================================================================
echo "" | tee -a "$SUMMARY"
echo -e "${BOLD}===== 附加测试: Clockwise + 禁用 RO =====${NC}" | tee -a "$SUMMARY"

run_and_measure "500_clockwise_noro" \
    RIVE_RENDER_WIDTH=500 RIVE_RENDER_HEIGHT=500 RIVE_CLOCKWISE=1 RIVE_DISABLE_RO=1
FPS_500_CW_NORO=$(cat "${RESULTS_DIR}/.last_fps")

# ===========================================================================
# FPS 对比汇总
# ===========================================================================
echo "" | tee -a "$SUMMARY"
echo "=============================================" | tee -a "$SUMMARY"
echo -e "${BOLD} FPS 对比汇总表${NC}" | tee -a "$SUMMARY"
echo "=============================================" | tee -a "$SUMMARY"
printf "%-15s %12s %12s %12s\n" "分辨率" "RasterOrder" "Clockwise" "变化" | tee -a "$SUMMARY"
printf "%-15s %12s %12s %12s\n" "---------------" "------------" "------------" "------------" | tee -a "$SUMMARY"

# Calculate percentage change
calc_change() {
    local base="$1"
    local new="$2"
    if [ "$base" = "N/A" ] || [ "$new" = "N/A" ]; then
        echo "N/A"
    else
        awk "BEGIN { printf \"%+.1f%%\", 100.0 * ($new - $base) / $base }"
    fi
}

CHANGE_500=$(calc_change "$FPS_500_RO" "$FPS_500_CW")
CHANGE_384=$(calc_change "$FPS_384_RO" "$FPS_384_CW")
CHANGE_448=$(calc_change "$FPS_448_RO" "$FPS_448_CW")

printf "%-15s %12s %12s %12s\n" "500x500" "$FPS_500_RO" "$FPS_500_CW" "$CHANGE_500" | tee -a "$SUMMARY"
printf "%-15s %12s %12s %12s\n" "448x448" "$FPS_448_RO" "$FPS_448_CW" "$CHANGE_448" | tee -a "$SUMMARY"
printf "%-15s %12s %12s %12s\n" "384x384" "$FPS_384_RO" "$FPS_384_CW" "$CHANGE_384" | tee -a "$SUMMARY"
printf "%-15s %12s %12s %12s\n" "500 CW+noRO" "-" "$FPS_500_CW_NORO" "-" | tee -a "$SUMMARY"

# ===========================================================================
# 视觉正确性检查指南
# ===========================================================================
echo "" | tee -a "$SUMMARY"
echo "=============================================" | tee -a "$SUMMARY"
echo " 视觉正确性验证清单 (需人工目视)" | tee -a "$SUMMARY"
echo "=============================================" | tee -a "$SUMMARY"
cat << 'CHECKLIST' | tee -a "$SUMMARY"

Clockwise 模式会强制所有路径使用顺时针填充规则 (非零绕组数 >= 0),
这可能破坏以下内容的正确渲染:

  [ ] 1. Even-Odd 填充规则的路径
       - 检查: 是否有本应镂空的区域变成实心?
       - 典型表现: 圆环/甜甜圈形状变成实心圆

  [ ] 2. 负绕组数区域
       - 检查: 逆时针绘制的区域是否消失或变形?
       - 典型表现: 重叠路径的减法区域不再生效

  [ ] 3. 复杂自交叉路径
       - 检查: 自交叉区域的填充是否正确?
       - 典型表现: 交叉处出现意外的实心/空白区域

  [ ] 4. 嵌套裁剪
       - 检查: 多层裁剪区域是否正确?
       - 典型表现: 裁剪边界出现锯齿或泄漏

  [ ] 5. 整体色彩和透明度
       - 检查: 整体颜色是否正确, Alpha 混合是否正常?
       - 典型表现: 颜色偏差, 半透明区域不正确

测试方法:
  1. 在 RIVE_CLOCKWISE=0 下运行, 拍照/截图作为参考
  2. 在 RIVE_CLOCKWISE=1 下运行, 拍照/截图
  3. 逐项对比以上清单中的视觉元素

如果所有项目通过:
  -> Clockwise 模式可安全启用, 享受 PLS 操作减少带来的性能提升

如果有任何项目失败:
  -> 需要分析具体失败的内容特征
  -> 考虑内容级别的修复 (如避免 even-odd 填充) 或仅在特定内容上启用

CHECKLIST

echo "" | tee -a "$SUMMARY"
ok "测试完成! 结果保存在: $RESULTS_DIR"
echo ""
echo "重要提示:"
echo "  FPS 变化表明了 Clockwise 模式的性能收益"
echo "  但视觉正确性需要人工目视验证 (见上方清单)"
echo "  建议: 同时运行两种模式, 对比屏幕输出"
