#!/bin/bash
# ==============================================================================
# check_env.sh — VLSI FPGA 大作业 环境检测脚本 (Bash)
# 功能:
#   1. 检测 g++ 编译器和版本
#   2. 检测 Python3 解释器
#   3. 检测 make 工具
#   4. 检测 testbench 数据目录完整性
#   5. 检测 lab2 已有的 benchmark 文件
# 用法:
#   bash check_env.sh
# 返回值:
#   0 — 所有检查通过
#   1 — 存在警告（非致命）
#   2 — 存在致命错误
# ==============================================================================

set -euo pipefail

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

PASS=0
WARN=0
FAIL=0
TOTAL=0

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
TIMESTAMP=$(date '+%Y-%m-%d %H:%M:%S')

echo "=============================================="
echo "  VLSI FPGA 大作业 — 环境检测"
echo "  时间: $TIMESTAMP"
echo "  项目目录: $PROJECT_DIR"
echo "=============================================="
echo ""

# -------------------------------------------------------------------
# 检测函数
# -------------------------------------------------------------------
check_cmd() {
    local name=$1
    local cmd=$2
    local version_arg=${3:---version}
    TOTAL=$((TOTAL + 1))

    if command -v "$cmd" &>/dev/null; then
        local ver
        ver=$($cmd "$version_arg" 2>&1 | head -n 1)
        echo -e "  [${GREEN}PASS${NC}] $name: $ver"
        PASS=$((PASS + 1))
        return 0
    else
        if [ "${4:-}" = "optional" ]; then
            echo -e "  [${YELLOW}WARN${NC}] $name: 未找到 (可选)"
            WARN=$((WARN + 1))
            return 1
        else
            echo -e "  [${RED}FAIL${NC}] $name: 未找到，请安装 $name"
            FAIL=$((FAIL + 1))
            return 2
        fi
    fi
}

check_file() {
    local desc=$1
    local path=$2
    local required=${3:-true}
    TOTAL=$((TOTAL + 1))

    if [ -f "$path" ]; then
        local size
        size=$(wc -c < "$path")
        echo -e "  [${GREEN}PASS${NC}] $desc: 存在 ($(numfmt --to=iec $size 2>/dev/null || echo "$size bytes"))"
        PASS=$((PASS + 1))
        return 0
    elif [ -d "$path" ]; then
        echo -e "  [${GREEN}PASS${NC}] $desc: 目录存在"
        PASS=$((PASS + 1))
        return 0
    else
        if [ "$required" = "true" ]; then
            echo -e "  [${RED}FAIL${NC}] $desc: 未找到 — $path"
            FAIL=$((FAIL + 1))
            return 2
        else
            echo -e "  [${YELLOW}WARN${NC}] $desc: 未找到 (可选) — $path"
            WARN=$((WARN + 1))
            return 1
        fi
    fi
}

check_gpp_version() {
    TOTAL=$((TOTAL + 1))
    if command -v g++ &>/dev/null; then
        local raw_ver
        raw_ver=$(g++ -std=c++17 -x c++ -E -dM /dev/null 2>&1 | grep __cplusplus | awk '{print $3}')
        # 去掉可能的 'L' 后缀 (如 201703L -> 201703)
        local ver="${raw_ver%L}"
        if [ "$ver" -ge "201703" ] 2>/dev/null; then
            echo -e "  [${GREEN}PASS${NC}] C++17 标准支持: 是 (__cplusplus = $raw_ver)"
            PASS=$((PASS + 1))
            return 0
        else
            echo -e "  [${YELLOW}WARN${NC}] C++17 支持: g++ 版本可能过旧 (__cplusplus = $raw_ver)"
            WARN=$((WARN + 1))
            return 1
        fi
    else
        echo -e "  [${RED}FAIL${NC}] C++17 检查: g++ 未安装"
        FAIL=$((FAIL + 1))
        return 2
    fi
}

echo "--- [1/4] 编译工具链检查 ---"
check_cmd "g++" "g++"
check_gpp_version
check_cmd "make" "make"
check_cmd "Python3" "python3"
echo ""

echo "--- [2/4] 项目代码检查 ---"
check_file "lab2/Solution.cpp" "$PROJECT_DIR/lab2/Solution.cpp"
check_file "lab2/Arch.h" "$PROJECT_DIR/lab2/Arch.h"
check_file "lab2/Object.h" "$PROJECT_DIR/lab2/Object.h"
check_file "lab2/Global.h" "$PROJECT_DIR/lab2/Global.h"
check_file "lab2/makefile" "$PROJECT_DIR/lab2/makefile"
echo ""

echo "--- [3/4] 测试数据检查 ---"
check_file "FPGA-example1" "$PROJECT_DIR/testbench/FPGA-example1"
check_file "FPGA-example1/design.nodes" "$PROJECT_DIR/testbench/FPGA-example1/design.nodes"
check_file "FPGA-example1/design.nets" "$PROJECT_DIR/testbench/FPGA-example1/design.nets"
check_file "FPGA-example1/design.pl" "$PROJECT_DIR/testbench/FPGA-example1/design.pl"
check_file "FPGA-example1/design.scl" "$PROJECT_DIR/testbench/FPGA-example1/design.scl"
check_file "FPGA-example1/design.lib" "$PROJECT_DIR/testbench/FPGA-example1/design.lib" "optional"
check_file "FPGA-example2" "$PROJECT_DIR/testbench/FPGA-example2"
check_file "FPGA-example3" "$PROJECT_DIR/testbench/FPGA-example3"
check_file "FPGA-example4" "$PROJECT_DIR/testbench/FPGA-example4"
echo ""

echo "--- [4/4] lab2 benchmark 检查 (可选) ---"
check_file "lab2/benchmark/small.txt" "$PROJECT_DIR/lab2/benchmark/small.txt" "optional"
check_file "lab2/benchmark/med1.txt" "$PROJECT_DIR/lab2/benchmark/med1.txt" "optional"
check_file "lab2/benchmark/xl.txt" "$PROJECT_DIR/lab2/benchmark/xl.txt" "optional"
check_file "lab2/benchmark/huge.txt" "$PROJECT_DIR/lab2/benchmark/huge.txt" "optional"
echo ""

# -------------------------------------------------------------------
# 汇总
# -------------------------------------------------------------------
echo "=============================================="
echo -e "  结果汇总: ${GREEN}$PASS PASS${NC}, ${YELLOW}$WARN WARN${NC}, ${RED}$FAIL FAIL${NC} (共 $TOTAL 项)"
echo "=============================================="

if [ "$FAIL" -gt 0 ]; then
    echo -e "${RED}⚠  存在 $FAIL 项致命错误，请修复后重试${NC}"
    exit 2
elif [ "$WARN" -gt 0 ]; then
    echo -e "${YELLOW}⚠  存在 $WARN 项警告，但不影响运行${NC}"
    exit 1
else
    echo -e "${GREEN}✅ 所有检查通过，环境就绪！${NC}"
    exit 0
fi
