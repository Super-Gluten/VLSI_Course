# #!/bin/bash
# # ==============================================================================
# # run_all.sh — VLSI FPGA 大作业 一键回归测试脚本 (Bash)
# #
# # 工作流:
# #   1. 环境检测 (调用 check_env.sh)
# #   2. 编译 (调用 make)
# #   3. 遍历 testbench/ 下所有 FPGA-example case 运行
# #   4. 汇总 HPWL 结果
# #
# # 用法:
# #   bash run_all.sh                    # 运行全部 4 个 case
# #   bash run_all.sh FPGA-example1      # 只运行指定 case
# #   bash run_all.sh --skip-env         # 跳过环境检测
# #
# # 返回值:
# #   0 — 所有 case 通过
# #   1 — 部分 case 失败
# #   2 — 编译或环境检测失败
# # ==============================================================================

# set -euo pipefail

# SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
# BUILD_DIR="$SCRIPT_DIR/build"
# OUTPUT_DIR="$SCRIPT_DIR/output"
# TIMESTAMP=$(date '+%Y-%m-%d_%H-%M-%S')
# LOG_DIR="$SCRIPT_DIR/logs/$TIMESTAMP"

# # 颜色
# RED='\033[0;31m'
# GREEN='\033[0;32m'
# YELLOW='\033[1;33m'
# CYAN='\033[0;36m'
# NC='\033[0m'

# PASS_COUNT=0
# FAIL_COUNT=0
# TOTAL_COUNT=0
# declare -A RESULTS

# # ==============================================================================
# # 辅助函数
# # ==============================================================================
# log()    { echo -e "${GREEN}[INFO]${NC} $1"; }
# warn()   { echo -e "${YELLOW}[WARN]${NC} $1"; }
# error()  { echo -e "${RED}[ERROR]${NC} $1"; }
# header() { echo -e "\n${CYAN}==============================================${NC}"; }
# subheader() { echo -e "${CYAN}---${NC} $1"; }

# check_ret() {
#     if [ $1 -ne 0 ]; then
#         error "$2"
#         exit 2
#     fi
# }

# # ==============================================================================
# # 阶段 1: 环境检测
# # ==============================================================================
# run_env_check() {
#     if [ "${SKIP_ENV:-false}" = "true" ]; then
#         log "跳过环境检测 (--skip-env)"
#         return 0
#     fi
#     header
#     echo "  阶段 1/5: 环境检测"
#     header
#     bash "$SCRIPT_DIR/check_env.sh"
#     local ret=$?
#     if [ $ret -eq 2 ]; then
#         error "环境检测失败，请修复后重试"
#         exit 2
#     fi
#     log "环境检测完成 (警告可忽略)"
# }

# # ==============================================================================
# # 阶段 2: 编译
# # ==============================================================================
# run_build() {
#     header
#     echo "  阶段 2/5: 编译"
#     header

#     mkdir -p "$BUILD_DIR"
#     cd "$SCRIPT_DIR"

#     # 检查是否需要 Makefile
#     if [ ! -f "makefile" ] && [ ! -f "Makefile" ]; then
#         error "未找到 makefile，请先生成"
#         exit 2
#     fi

#     make clean 2>/dev/null || true
#     make -j"$(nproc)" 2>&1 | tee "$LOG_DIR/build.log"
#     local ret=${PIPESTATUS[0]}

#     if [ $ret -ne 0 ]; then
#         error "编译失败，详情见 $LOG_DIR/build.log"
#         exit 2
#     fi
#     log "编译成功"

#     # 检查可执行文件是否存在
#     if [ ! -f "$SCRIPT_DIR/main" ]; then
#         error "编译成功但未找到 ./main 可执行文件"
#         exit 2
#     fi
# }

# # ==============================================================================
# # 阶段 3: 运行单个 Case
# # ==============================================================================
# run_case() {
#     local case_name=$1
#     local case_dir="$PROJECT_DIR/testbench/$case_name"

#     TOTAL_COUNT=$((TOTAL_COUNT + 1))

#     if [ ! -d "$case_dir" ]; then
#         error "Case 目录不存在: $case_dir"
#         RESULTS["$case_name"]="SKIP"
#         return 1
#     fi

#     local output_file="$OUTPUT_DIR/${case_name}_placement.txt"
#     local case_log="$LOG_DIR/${case_name}.log"

#     subheader "运行 $case_name"
#     echo "  输入: $case_dir"
#     echo "  输出: $output_file"
#     echo "  日志: $case_log"

#     mkdir -p "$OUTPUT_DIR"

#     # 运行布局算法
#     set +e
#     (
#         cd "$SCRIPT_DIR"
#         timeout 600 ./main "$case_dir" "$output_file" 2>&1
#     ) > "$case_log" 2>&1
#     local ret=$?
#     set -e

#     if [ $ret -ne 0 ]; then
#         if [ $ret -eq 124 ]; then
#             error "$case_name: 运行超时 (600s)"
#         else
#             error "$case_name: 运行失败 (exit=$ret)"
#         fi
#         RESULTS["$case_name"]="FAIL"
#         FAIL_COUNT=$((FAIL_COUNT + 1))
#         return 1
#     fi

#     # 提取 HPWL
#     local hpwl=""
#     if grep -q "Wirelength:" "$case_log"; then
#         hpwl=$(grep "Wirelength:" "$case_log" | awk '{print $2}')
#     fi

#     local errors=""
#     if grep -q "errors" "$case_log"; then
#         errors=$(grep "errors" "$case_log" | grep -oP '\d+(?=\s*errors)' || echo "?")
#     fi

#     log "$case_name: HPWL=$hpwl, Errors=$errors"
#     RESULTS["$case_name"]="PASS"

#     if [ -n "$hpwl" ]; then
#         echo "$case_name $hpwl" >> "$LOG_DIR/hpwl_summary.tmp"
#     fi

#     PASS_COUNT=$((PASS_COUNT + 1))
#     return 0
# }

# # ==============================================================================
# # 阶段 4: 结果汇总
# # ==============================================================================
# print_summary() {
#     header
#     echo "  阶段 4/5: 结果汇总"
#     header

#     echo ""
#     echo "  ${CYAN}Case                  HPWL         Status${NC}"
#     echo "  ${CYAN}----------------------------------------------${NC}"

#     local total_hpwl=0
#     local count=0

#     while IFS= read -r line; do
#         local name=$(echo "$line" | awk '{print $1}')
#         local hpwl=$(echo "$line" | awk '{print $2}')
#         local status="${RESULTS[$name]:-N/A}"
#         local status_color="$GREEN"
#         if [ "$status" = "FAIL" ]; then
#             status_color="$RED"
#         fi
#         printf "  %-20s %-12s ${status_color}%s${NC}\n" "$name" "$hpwl" "$status"
#         if [ "$hpwl" != "N/A" ] && [ "$hpwl" != "" ]; then
#             total_hpwl=$((total_hpwl + hpwl))
#             count=$((count + 1))
#         fi
#     done < "$LOG_DIR/hpwl_summary.tmp"

#     echo "  ${CYAN}----------------------------------------------${NC}"
#     if [ $count -gt 0 ]; then
#         echo "  总 HPWL: $total_hpwl  (平均: $((total_hpwl / count)))"
#     fi
#     echo ""
#     echo -e "  通过: ${GREEN}$PASS_COUNT${NC} / $TOTAL_COUNT"
#     echo -e "  失败: ${RED}$FAIL_COUNT${NC} / $TOTAL_COUNT"

#     # 保存汇总到文件
#     cp "$LOG_DIR/hpwl_summary.tmp" "$OUTPUT_DIR/hpwl_summary.txt"
# }

# # ==============================================================================
# # 阶段 5: 写入记忆库快照
# # ==============================================================================
# write_memory_snapshot() {
#     header
#     echo "  阶段 5/5: 写入运行快照"
#     header

#     local snapshot_file="$SCRIPT_DIR/../plans/PERF_SNAPSHOT.md"

#     echo "# 性能快照 — $(date '+%Y-%m-%d %H:%M:%S')" > "$snapshot_file"
#     echo "" >> "$snapshot_file"
#     echo "| Case | HPWL | Status |" >> "$snapshot_file"
#     echo "|------|------|--------|" >> "$snapshot_file"

#     while IFS= read -r line; do
#         local name=$(echo "$line" | awk '{print $1}')
#         local hpwl=$(echo "$line" | awk '{print $2}')
#         local status="${RESULTS[$name]:-N/A}"
#         echo "| $name | $hpwl | $status |" >> "$snapshot_file"
#     done < "$LOG_DIR/hpwl_summary.tmp"

#     echo "" >> "$snapshot_file"
#     echo "_快照自动生成于 $(date '+%Y-%m-%d %H:%M:%S')_" >> "$snapshot_file"
#     log "快照已写入 $snapshot_file"
# }

# # ==============================================================================
# # 主流程
# # ==============================================================================
# main() {
#     # 解析参数
#     SKIP_ENV=false
#     SPECIFIC_CASE=""
#     for arg in "$@"; do
#         case "$arg" in
#             --skip-env) SKIP_ENV=true ;;
#             --help|-h)
#                 echo "用法: bash run_all.sh [--skip-env] [case_name]"
#                 echo "  --skip-env    跳过环境检测"
#                 echo "  case_name     只运行指定 case (如 FPGA-example1)"
#                 exit 0
#                 ;;
#             *)
#                 if [ -z "$SPECIFIC_CASE" ]; then
#                     SPECIFIC_CASE="$arg"
#                 fi
#                 ;;
#         esac
#     done

#     echo "=============================================="
#     echo "  VLSI FPGA 大作业 — 一键回归测试"
#     echo "  开始时间: $(date '+%Y-%m-%d %H:%M:%S')"
#     echo "  项目目录: $PROJECT_DIR"
#     echo "=============================================="

#     # 创建日志目录
#     mkdir -p "$LOG_DIR"
#     > "$LOG_DIR/hpwl_summary.tmp"

#     # 阶段 1: 环境检测
#     run_env_check

#     # 阶段 2: 编译
#     run_build

#     # 阶段 3: 运行
#     header
#     echo "  阶段 3/5: 布局算法运行"
#     header

#     if [ -n "$SPECIFIC_CASE" ]; then
#         log "单 case 模式: $SPECIFIC_CASE"
#         run_case "$SPECIFIC_CASE"
#     else
#         for case in "FPGA-example1" "FPGA-example2" "FPGA-example3" "FPGA-example4"; do
#             run_case "$case"
#         done
#     fi

#     # 阶段 4: 汇总
#     print_summary

#     # 阶段 5: 快照
#     write_memory_snapshot

#     # 最终结果
#     echo ""
#     if [ $FAIL_COUNT -eq 0 ]; then
#         echo -e "${GREEN}==============================================${NC}"
#         echo -e "${GREEN}  ✅ 全部 $TOTAL_COUNT 个 case 通过${NC}"
#         echo -e "${GREEN}==============================================${NC}"
#         exit 0
#     else
#         echo -e "${RED}==============================================${NC}"
#         echo -e "${RED}  ⚠  $FAIL_COUNT / $TOTAL_COUNT 个 case 失败${NC}"
#         echo -e "${RED}==============================================${NC}"
#         exit 1
#     fi
# }

# main "$@"
cd /home/zhm/VLSI-FPGA/labFinal && make clean 2>/dev/null; make lab_main 2>&1 | tail -3

echo =========FPGA-example1=========
./lab_main ../testbench/FPGA-example1 ./output/FPGA-example1_placement.txt

echo =========FPGA-example2=========
./lab_main ../testbench/FPGA-example2 ./output/FPGA-example2_placement.txt

echo =========FPGA-example3=========
./lab_main ../testbench/FPGA-example3 ./output/FPGA-example3_placement.txt

echo =========FPGA-example4=========
./lab_main ../testbench/FPGA-example3 ./output/FPGA-example4_placement.txt