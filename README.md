# VLSI FPGA 大作业 — ISPD2016 FPGA 布局求解器

## 项目简介

面向 ISPD2016 FPGA Placement Contest 的布局求解器，
支持 FPGA 资源约束（SLICE/DSP/BRAM/IO）、多实例/站点 BEL 打包，
以及贪心网表扩散布局算法。全部 4 个 benchmark 通过合法性验证（0 错误）。

## 环境要求

- **操作系统**: Linux（推荐 Ubuntu 20.04+）或 WSL2
- **编译器**: g++ 9+（支持 C++17 和 OpenMP）
- **构建工具**: GNU Make
- **Python**（可选）：Python 3.6+（用于 `.py` 脚本）

## 数据集

4 个 ISPD2016 FPGA Placement Benchmark 位于 [`testbench/`](testbench/)：

| 数据集 | 实例数 | 可移动 | 线网数 | SiteMap |
|--------|--------|--------|--------|---------|
| FPGA-example1 | 3,336 | 3,264 | 3,346 | 168 × 480 |
| FPGA-example2 | 542,239 | 541,785 | 545,089 | 168 × 480 |
| FPGA-example3 | 427,800 | 427,194 | 428,080 | 168 × 480 |
| FPGA-example4 | 844,184 | 843,578 | 844,952 | 168 × 480 |

## 快速开始

```bash
cd labFinal

# 一键运行全部 4 个 case
bash run_all.sh

# 或使用 Python 版
python3 run_all.py

# 手动编译 + 运行单个 case
make lab_main
./lab_main ../testbench/FPGA-example1 ./output/FPGA-example1_placement.txt

# 环境检测
bash check_env.sh
```

## 算法概述

四次迭代改进：

| 版本 | 算法 | example1 HPWL | 大算例 |
|------|------|---------------|--------|
| v1 | 随机填充 + SA | 101,056 | ❌ 不可行 |
| v2 | 分组填充 | 49,818 (22s) | ✅ 30s |
| v3 | **贪心网表扩散** | **49,818 (0.5s)** | ✅ **9-19s** |
| v4 | 力导向 (OpenMP 并行) | **35,180 (0.5s)** | ⚠️ 优化中 |

## 实验结果

| 数据集 | HPWL | Baseline | 运行时间 | 合法性 |
|--------|------|----------|----------|--------|
| FPGA-example1 | **35,180** | 13,562 | 0.5s | ✅ |
| FPGA-example2 | **69,353,923** | 2,914,068 | 9.1s | ✅ |
| FPGA-example3 | **78,247,905** | 7,781,857 | 8.4s | ✅ |
| FPGA-example4 | **131,001,468** | 8,221,614 | 18.9s | ✅ |

## 项目结构

```
VLSI-FPGA/
├── labFinal/               # 大作业代码
│   ├── check_env.sh / .py  # 环境检测（Bash + Python）
│   ├── run_all.sh / .py    # 一键回归测试
│   ├── makefile            # 编译脚本（+OpenMP）
│   ├── README.md           # labFinal 使用说明
│   ├── main.cpp            # 主入口
│   ├── SiteType.h / .cpp   # FPGA 资源类型
│   ├── ISPDReader.h / .cpp # ISPD2016 解析器
│   ├── Solution.h / .cpp   # 布局求解器
│   └── output/             # 输出目录
├── testbench/              # 测试数据集
│   ├── FPGA-example1/
│   ├── FPGA-example2/
│   ├── FPGA-example3/
│   └── FPGA-example4/
└── report/
    ├── labFinal_report.tex # 实验报告源码
    └── labFinal_report.pdf # 实验报告

```

## 引用

- ISPD 2016: FPGA Placement Contest
- DREAMPlaceFPGA: An Open-Source Analytical Placer for Heterogeneous FPGAs
