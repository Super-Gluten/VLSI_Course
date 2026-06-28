# ==============================================================================
# Makefile — VLSI FPGA 大作业 编译脚本
#
# 目标:
#   make all      — 编译全部并运行 4 个 FPGA case
#   make main     — 编译生成可执行文件
#   make run      — 运行全部 4 个 FPGA case
#   make case1    — 仅运行 FPGA-example1
#   make case2    — 仅运行 FPGA-example2
#   make case3    — 仅运行 FPGA-example3
#   make case4    — 仅运行 FPGA-example4
#   make env      — 环境检测
#   make clean    — 清理编译产物
#   make help     — 帮助信息
# ==============================================================================

# ---------- 编译器与标志 ----------
CXX      := g++
CXXFLAGS := -Wall -Wextra -std=c++17 -O2 -g
LDFLAGS  :=

# ---------- 目录 ----------
SRC_DIR  := .
OBJ_DIR  := ./build
OUT_DIR  := ./output
LOG_DIR  := ./logs
TB_DIR   := ../testbench
PROJ_DIR := ..

# ---------- 源文件 ----------
SRCS := \
	main.cpp \
	SiteType.cpp \
	Solution.cpp \
	ISPDReader.cpp

OBJS := $(SRCS:%.cpp=$(OBJ_DIR)/%.o)
DEPS := $(OBJS:.o=.d)
TARGET := lab_main

# ---------- 默认目标 ----------
all: env_check $(TARGET) run_all

# ---------- 编译规则 ----------
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "  ✅ 编译成功: $@"

# 自动包含依赖
-include $(DEPS)

# ---------- 编译可执行文件 ----------
main: $(TARGET)

# ---------- 运行 ----------
run_all: $(TARGET)
	@mkdir -p $(OUT_DIR) $(LOG_DIR)
	@echo ""
	@echo "=============================================="
	@echo "  运行全部 4 个 FPGA case"
	@echo "=============================================="
	@total=0; pass=0; fail=0; \
	for case in FPGA-example1 FPGA-example2 FPGA-example3 FPGA-example4; do \
	    total=$$((total+1)); \
	    case_dir="$(TB_DIR)/$$case"; \
	    output="$(OUT_DIR)/$${case}_placement.txt"; \
	    log="$(LOG_DIR)/$${case}.log"; \
	    echo "--- [$$case] ---"; \
	    timeout 600 ./$(TARGET) "$$case_dir" "$$output" > "$$log" 2>&1; \
	    ret=$$?; \
	    if [ $$ret -eq 0 ]; then \
	        hpwl=$$(grep "Wirelength:" "$$log" | awk '{print $$2}'); \
	        echo "  ✅ $$case: HPWL=$$hpwl"; \
	        pass=$$((pass+1)); \
	    elif [ $$ret -eq 124 ]; then \
	        echo "  ❌ $$case: 超时"; \
	        fail=$$((fail+1)); \
	    else \
	        echo "  ❌ $$case: 失败 (exit=$$ret)"; \
	        fail=$$((fail+1)); \
	    fi; \
	    echo ""; \
	done; \
	echo "=============================================="; \
	echo "  结果: $$pass PASS / $$fail FAIL (共 $$total)"; \
	echo "=============================================="

# 单 case 运行
case1: $(TARGET)
	@mkdir -p $(OUT_DIR) $(LOG_DIR)
	./$(TARGET) "$(TB_DIR)/FPGA-example1" "$(OUT_DIR)/FPGA-example1_placement.txt" \
	    | tee "$(LOG_DIR)/FPGA-example1.log"

case2: $(TARGET)
	@mkdir -p $(OUT_DIR) $(LOG_DIR)
	./$(TARGET) "$(TB_DIR)/FPGA-example2" "$(OUT_DIR)/FPGA-example2_placement.txt" \
	    | tee "$(LOG_DIR)/FPGA-example2.log"

case3: $(TARGET)
	@mkdir -p $(OUT_DIR) $(LOG_DIR)
	./$(TARGET) "$(TB_DIR)/FPGA-example3" "$(OUT_DIR)/FPGA-example3_placement.txt" \
	    | tee "$(LOG_DIR)/FPGA-example3.log"

case4: $(TARGET)
	@mkdir -p $(OUT_DIR) $(LOG_DIR)
	./$(TARGET) "$(TB_DIR)/FPGA-example4" "$(OUT_DIR)/FPGA-example4_placement.txt" \
	    | tee "$(LOG_DIR)/FPGA-example4.log"

# ---------- 环境检测 ----------
env_check:
	@echo "--- 环境检测 ---"
	@command -v g++ >/dev/null 2>&1 && echo "  ✅ g++: $$(g++ --version | head -1)" \
	    || { echo "  ❌ g++ 未找到"; exit 1; }
	@command -v make >/dev/null 2>&1 && echo "  ✅ make: $$(make --version | head -1)" \
	    || { echo "  ❌ make 未找到"; exit 1; }
	@echo "--- 测试数据检查 ---"
	@for case in FPGA-example1 FPGA-example2 FPGA-example3 FPGA-example4; do \
	    if [ -d "$(TB_DIR)/$$case" ]; then \
	        echo "  ✅ $$case: 存在"; \
	    else \
	        echo "  ❌ $$case: 未找到"; \
	        exit 1; \
	    fi; \
	done
	@echo "  ✅ 环境就绪"

# ---------- 清理 ----------
clean:
	rm -rf $(OBJ_DIR) $(TARGET)
	@echo "  ✅ 清理完成"

distclean: clean
	rm -rf $(OUT_DIR) $(LOG_DIR)
	@echo "  ✅ 完全清理完成"

# ---------- 帮助 ----------
help:
	@echo "用法:"
	@echo "  make all       — 编译 + 运行全部 4 个 case"
	@echo "  make main      — 仅编译"
	@echo "  make run_all   — 运行全部 4 个 case"
	@echo "  make case1-4   — 运行指定 case"
	@echo "  make env       — 环境检测"
	@echo "  make clean     — 清理编译产物"
	@echo "  make distclean — 清理全部产物"
	@echo "  make help      — 本帮助"

.PHONY: all main run_all env_check clean distclean help \
        case1 case2 case3 case4
