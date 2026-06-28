#!/usr/bin/env python3
"""
check_env.py — VLSI FPGA 大作业 环境检测脚本 (Python 跨平台版)

功能:
  1. 检测 g++ 编译器和 C++17 标准支持
  2. 检测 Python3 解释器版本
  3. 检测 make 工具
  4. 检测 testbench 数据目录完整性
  5. 检测 lab2 已有的 benchmark 文件
  6. 跨平台兼容 (Windows/Linux/macOS)

用法:
  python3 check_env.py

返回值:
  0 — 所有检查通过
  1 — 存在警告（非致命）
  2 — 存在致命错误
"""

import os
import sys
import shutil
import subprocess
import platform
from pathlib import Path

# ==============================================================================
# 配置
# ==============================================================================
SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_DIR = SCRIPT_DIR.parent

REQUIRED_FILES = {
    "lab2/Solution.cpp": True,
    "lab2/Arch.h": True,
    "lab2/Object.h": True,
    "lab2/Global.h": True,
    "lab2/makefile": True,
}

TESTBENCH_CASES = ["FPGA-example1", "FPGA-example2", "FPGA-example3", "FPGA-example4"]
TESTBENCH_FILES = ["design.nodes", "design.nets", "design.pl", "design.scl"]

OPTIONAL_BENCHMARKS = [
    "lab2/benchmark/small.txt",
    "lab2/benchmark/med1.txt",
    "lab2/benchmark/med2.txt",
    "lab2/benchmark/lg1.txt",
    "lab2/benchmark/lg2.txt",
    "lab2/benchmark/xl.txt",
    "lab2/benchmark/huge.txt",
]

# ==============================================================================
# 检测引擎
# ==============================================================================
class EnvChecker:
    def __init__(self):
        self.passed = 0
        self.warned = 0
        self.failed = 0
        self.total = 0

    def _color(self, text, color):
        if not sys.stdout.isatty():
            return text
        colors = {"red": "31", "green": "32", "yellow": "33", "cyan": "36"}
        return f"\033[{colors.get(color, '0')}m{text}\033[0m"

    def check_cmd(self, name: str, cmd: str, version_arg: str = "--version",
                  required: bool = True) -> bool:
        """检查系统命令是否存在"""
        self.total += 1
        cmd_path = shutil.which(cmd)
        if cmd_path:
            try:
                ver = subprocess.check_output(
                    [cmd, version_arg], stderr=subprocess.STDOUT,
                    timeout=10
                ).decode("utf-8", errors="replace").split("\n")[0].strip()
                print(f"  [{self._color('PASS', 'green')}] {name}: {ver}")
                self.passed += 1
                return True
            except (subprocess.CalledProcessError, OSError):
                print(f"  [{self._color('PASS', 'green')}] {name}: 找到 ({cmd_path})")
                self.passed += 1
                return True
        else:
            if required:
                print(f"  [{self._color('FAIL', 'red')}] {name}: 未找到，请安装 {name}")
                self.failed += 1
                return False
            else:
                print(f"  [{self._color('WARN', 'yellow')}] {name}: 未找到 (可选)")
                self.warned += 1
                return False

    def check_cpp17(self) -> bool:
        """检测 g++ 是否支持 C++17"""
        self.total += 1
        gpp = shutil.which("g++")
        if not gpp:
            print(f"  [{self._color('FAIL', 'red')}] C++17 检查: g++ 未安装")
            self.failed += 1
            return False

        try:
            output = subprocess.check_output(
                [gpp, "-std=c++17", "-x", "c++", "-E", "-dM", "/dev/null"],
                stderr=subprocess.STDOUT, timeout=10
            ).decode("utf-8")
            for line in output.splitlines():
                if "__cplusplus" in line:
                    # 去掉可能的 'L' 后缀 (如 201703L -> 201703)
                    ver_str = line.split()[-1].rstrip('L')
                    ver = int(ver_str)
                    if ver >= 201703:
                        print(f"  [{self._color('PASS', 'green')}] C++17 支持: 是 "
                              f"(__cplusplus = {ver})")
                        self.passed += 1
                        return True
                    else:
                        print(f"  [{self._color('WARN', 'yellow')}] C++17 支持: "
                              f"版本可能过旧 (__cplusplus = {ver})")
                        self.warned += 1
                        return False
            print(f"  [{self._color('WARN', 'yellow')}] C++17 支持: 无法确定版本")
            self.warned += 1
            return False
        except (subprocess.CalledProcessError, OSError) as e:
            print(f"  [{self._color('FAIL', 'red')}] C++17 检查失败: {e}")
            self.failed += 1
            return False

    def check_file(self, desc: str, path: Path, required: bool = True) -> bool:
        """检查文件或目录是否存在"""
        self.total += 1
        if path.exists():
            if path.is_file():
                size = path.stat().st_size
                size_str = self._format_size(size)
                print(f"  [{self._color('PASS', 'green')}] {desc}: 存在 ({size_str})")
            else:
                print(f"  [{self._color('PASS', 'green')}] {desc}: 目录存在")
            self.passed += 1
            return True
        else:
            if required:
                print(f"  [{self._color('FAIL', 'red')}] {desc}: 未找到 — {path}")
                self.failed += 1
                return False
            else:
                print(f"  [{self._color('WARN', 'yellow')}] {desc}: 未找到 (可选) — {path}")
                self.warned += 1
                return False

    @staticmethod
    def _format_size(bytes_val: int) -> str:
        """格式化文件大小"""
        for unit in ['B', 'KB', 'MB', 'GB']:
            if bytes_val < 1024:
                return f"{bytes_val:.1f}{unit}"
            bytes_val /= 1024
        return f"{bytes_val:.1f}TB"

    def run(self):
        """执行所有检查"""
        os_name = platform.system()
        print("=" * 50)
        print(f"  VLSI FPGA 大作业 — 环境检测")
        print(f"  时间: {subprocess.check_output(['date']).decode().strip()}")
        print(f"  系统: {os_name} {platform.release()}")
        print(f"  项目目录: {PROJECT_DIR}")
        print("=" * 50)
        print()

        # [1/4] 编译工具链
        print("--- [1/4] 编译工具链检查 ---")
        self.check_cmd("g++", "g++")
        self.check_cpp17()
        self.check_cmd("make", "make")
        self.check_cmd("Python3", "python3")
        print()

        # [2/4] 项目代码
        print("--- [2/4] 项目代码检查 ---")
        for rel_path, required in REQUIRED_FILES.items():
            self.check_file(rel_path, PROJECT_DIR / rel_path, required)
        print()

        # [3/4] 测试数据
        print("--- [3/4] 测试数据检查 ---")
        for case in TESTBENCH_CASES:
            case_dir = PROJECT_DIR / "testbench" / case
            self.check_file(case, case_dir, required=True)
            for file in TESTBENCH_FILES:
                self.check_file(f"{case}/{file}", case_dir / file, required=True)
        print()

        # [4/4] lab2 benchmark (可选)
        print("--- [4/4] lab2 benchmark 检查 (可选) ---")
        for rel_path in OPTIONAL_BENCHMARKS:
            self.check_file(rel_path, PROJECT_DIR / rel_path, required=False)
        print()

        # 汇总
        print("=" * 50)
        print(f"  结果汇总: {self._color(f'{self.passed} PASS', 'green')}, "
              f"{self._color(f'{self.warned} WARN', 'yellow')}, "
              f"{self._color(f'{self.failed} FAIL', 'red')} "
              f"(共 {self.total} 项)")
        print("=" * 50)

        if self.failed > 0:
            print(f"{self._color('⚠  存在 {} 项致命错误，请修复后重试'.format(self.failed), 'red')}")
            return 2
        elif self.warned > 0:
            print(f"{self._color('⚠  存在 {} 项警告，但不影响运行'.format(self.warned), 'yellow')}")
            return 1
        else:
            print(f"{self._color('✅ 所有检查通过，环境就绪！', 'green')}")
            return 0


def main():
    checker = EnvChecker()
    sys.exit(checker.run())


if __name__ == "__main__":
    main()
