#!/usr/bin/env python3
"""
run_all.py — VLSI FPGA 大作业 一键回归测试脚本 (Python 跨平台版)

工作流:
  1. 环境检测 (调用 check_env.py)
  2. 编译 (调用 make)
  3. 遍历 testbench/ 下所有 FPGA-example case 运行
  4. 汇总 HPWL 结果并输出结构化报告
  5. 可选：并行运行多个 case

用法:
  python3 run_all.py                        # 运行全部 4 个 case
  python3 run_all.py FPGA-example1          # 只运行指定 case
  python3 run_all.py --parallel             # 并行运行
  python3 run_all.py --skip-env             # 跳过环境检测
  python3 run_all.py --json                 # 输出 JSON 格式结果

返回值:
  0 — 所有 case 通过
  1 — 部分 case 失败
  2 — 编译或环境检测失败
"""

import argparse
import json
import os
import subprocess
import sys
import time
import threading
from concurrent.futures import ThreadPoolExecutor, as_completed
from datetime import datetime
from pathlib import Path

# ==============================================================================
# 配置
# ==============================================================================
SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_DIR = SCRIPT_DIR.parent
TESTBENCH_DIR = PROJECT_DIR / "testbench"
OUTPUT_DIR = SCRIPT_DIR / "output"

TEST_CASES = ["FPGA-example1", "FPGA-example2", "FPGA-example3", "FPGA-example4"]
TIMEOUT_SECONDS = 600  # 10 分钟每 case

# ==============================================================================
# 工具函数
# ==============================================================================
def color(text, color_name):
    colors = {"red": "31", "green": "32", "yellow": "33", "cyan": "36"}
    if not sys.stdout.isatty():
        return text
    return f"\033[{colors.get(color_name, '0')}m{text}\033[0m"


def log(msg):
    print(f"{color('[INFO]', 'green')} {msg}")


def warn(msg):
    print(f"{color('[WARN]', 'yellow')} {msg}")


def error(msg):
    print(f"{color('[ERROR]', 'red')} {msg}")


def header(title):
    print(f"\n{color('=' * 50, 'cyan')}")
    print(f"  {title}")
    print(f"{color('=' * 50, 'cyan')}")


# ==============================================================================
# 阶段 1: 环境检测
# ==============================================================================
def run_env_check(skip_env: bool = False):
    """调用 check_env.py 检测环境"""
    if skip_env:
        log("跳过环境检测 (--skip-env)")
        return True

    header("阶段 1/5: 环境检测")
    env_script = SCRIPT_DIR / "check_env.py"
    if not env_script.exists():
        warn(f"check_env.py 未找到: {env_script}，跳过环境检测")
        return True

    result = subprocess.run(
        [sys.executable, str(env_script)],
        capture_output=False, timeout=30
    )
    if result.returncode == 2:
        error("环境检测失败，请修复后重试")
        return False
    log("环境检测完成")
    return True


# ==============================================================================
# 阶段 2: 编译
# ==============================================================================
def run_build() -> bool:
    """编译项目"""
    header("阶段 2/5: 编译")

    if not (SCRIPT_DIR / "makefile").exists() and not (SCRIPT_DIR / "Makefile").exists():
        error("未找到 makefile，请先生成")
        return False

    # 清理
    subprocess.run(["make", "clean"], cwd=str(SCRIPT_DIR),
                   capture_output=True, timeout=30)

    # 编译
    log_dir = LOG_DIR
    build_log = log_dir / "build.log"
    nproc = os.cpu_count() or 4

    try:
        result = subprocess.run(
            ["make", f"-j{nproc}"],
            cwd=str(SCRIPT_DIR),
            capture_output=True, timeout=120
        )
        with open(build_log, "w") as f:
            f.write(result.stdout.decode())
            if result.stderr:
                f.write("\n--- STDERR ---\n")
                f.write(result.stderr.decode())

        if result.returncode != 0:
            error(f"编译失败，详情见 {build_log}")
            print(result.stderr.decode()[:500])
            return False

        log("编译成功")
        return True
    except subprocess.TimeoutExpired:
        error("编译超时 (120s)")
        return False


# ==============================================================================
# 阶段 3: 运行单个 Case
# ==============================================================================
def run_case(case_name: str, log_dir: Path) -> dict:
    """运行单个 case，返回结果字典"""
    case_dir = TESTBENCH_DIR / case_name
    output_file = OUTPUT_DIR / f"{case_name}_placement.txt"
    case_log = log_dir / f"{case_name}.log"

    if not case_dir.exists():
        error(f"Case 目录不存在: {case_dir}")
        return {"case": case_name, "status": "SKIP", "hpwl": None, "error": None}

    print(f"  {color('---', 'cyan')} 运行 {case_name}")

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    main_exe = SCRIPT_DIR / "main"
    if not main_exe.exists():
        error(f"可执行文件不存在: {main_exe}")
        return {"case": case_name, "status": "FAIL", "hpwl": None,
                "error": "可执行文件不存在"}

    start_time = time.time()
    try:
        result = subprocess.run(
            [str(main_exe), str(case_dir), str(output_file)],
            cwd=str(SCRIPT_DIR),
            capture_output=True, timeout=TIMEOUT_SECONDS
        )
        elapsed = time.time() - start_time

        # 保存日志
        with open(case_log, "w") as f:
            f.write(result.stdout.decode())
            if result.stderr:
                f.write("\n--- STDERR ---\n")
                f.write(result.stderr.decode())

        # 解析输出
        stdout = result.stdout.decode()
        hpwl = None
        errors = None

        for line in stdout.splitlines():
            if "Wirelength:" in line:
                try:
                    hpwl = int(line.split()[-1])
                except (ValueError, IndexError):
                    pass

        # 检查 exit code
        if result.returncode != 0:
            if result.returncode == -15:  # SIGTERM (timeout)
                error(f"{case_name}: 运行超时 ({TIMEOUT_SECONDS}s)")
                return {"case": case_name, "status": "FAIL", "hpwl": hpwl,
                        "error": "timeout"}
            else:
                error(f"{case_name}: 运行失败 (exit={result.returncode})")
                return {"case": case_name, "status": "FAIL", "hpwl": hpwl,
                        "error": f"exit={result.returncode}"}

        log(f"{case_name}: HPWL={hpwl}, 耗时={elapsed:.1f}s")
        return {"case": case_name, "status": "PASS", "hpwl": hpwl,
                "elapsed": round(elapsed, 1), "error": None}

    except subprocess.TimeoutExpired:
        elapsed = time.time() - start_time
        error(f"{case_name}: 运行超时 ({TIMEOUT_SECONDS}s)")
        return {"case": case_name, "status": "FAIL", "hpwl": None,
                "error": f"timeout ({elapsed:.0f}s)"}


# ==============================================================================
# 阶段 4: 结果汇总
# ==============================================================================
def print_summary(results: list, output_json: bool = False):
    """打印或导出结果汇总"""
    header("阶段 4/5: 结果汇总")

    pass_count = sum(1 for r in results if r["status"] == "PASS")
    fail_count = sum(1 for r in results if r["status"] == "FAIL")
    total = len(results)

    if output_json:
        summary = {
            "timestamp": datetime.now().isoformat(),
            "total": total,
            "passed": pass_count,
            "failed": fail_count,
            "results": results,
            "total_hpwl": sum(r["hpwl"] for r in results if r["hpwl"] is not None)
        }
        json_path = OUTPUT_DIR / "results.json"
        with open(json_path, "w") as f:
            json.dump(summary, f, indent=2)
        print(json.dumps(summary, indent=2))
        log(f"JSON 结果已导出: {json_path}")
        return summary

    print(f"\n  {'Case':<20} {'HPWL':<12} {'耗时(s)':<10} {'Status':<8}")
    print(f"  {'-' * 55}")

    total_hpwl = 0
    count = 0

    for r in results:
        status_str = r["status"]
        if status_str == "PASS":
            status_str = color("PASS", "green")
        elif status_str == "FAIL":
            status_str = color("FAIL", "red")
        else:
            status_str = color("SKIP", "yellow")

        hpwl_str = str(r["hpwl"]) if r["hpwl"] is not None else "N/A"
        elapsed_str = str(r.get("elapsed", "")) if r.get("elapsed") else "N/A"

        print(f"  {r['case']:<20} {hpwl_str:<12} {elapsed_str:<10} {status_str}")

        if r["hpwl"] is not None:
            total_hpwl += r["hpwl"]
            count += 1

    print(f"  {'-' * 55}")
    if count > 0:
        avg = total_hpwl // count
        print(f"  总 HPWL: {total_hpwl}  (平均: {avg})")
    print(f"\n  通过: {color(str(pass_count), 'green')} / {total}")
    print(f"  失败: {color(str(fail_count), 'red')} / {total}")

    return {
        "total": total,
        "passed": pass_count,
        "failed": fail_count,
        "total_hpwl": total_hpwl,
        "results": results
    }


# ==============================================================================
# 阶段 5: 写入记忆库快照
# ==============================================================================
def write_memory_snapshot(results: list, summary: dict):
    """将运行结果写入 plans/PERF_SNAPSHOT.md"""
    header("阶段 5/5: 写入运行快照")

    snapshot_path = PROJECT_DIR / "plans" / "PERF_SNAPSHOT.md"
    now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    lines = [
        f"# 性能快照 — {now}\n",
        "| Case | HPWL | 耗时(s) | Status |",
        "|------|------|---------|--------|",
    ]

    for r in results:
        hpwl = str(r["hpwl"]) if r["hpwl"] is not None else "N/A"
        elapsed = str(r.get("elapsed", "")) if r.get("elapsed") else "N/A"
        lines.append(f"| {r['case']} | {hpwl} | {elapsed} | {r['status']} |")

    lines.append(f"\n_快照自动生成于 {now}_\n")

    with open(snapshot_path, "w") as f:
        f.write("\n".join(lines) + "\n")

    log(f"快照已写入 {snapshot_path}")


# ==============================================================================
# 主流程
# ==============================================================================
def main():
    parser = argparse.ArgumentParser(
        description="VLSI FPGA 大作业 — 一键回归测试脚本",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
用例:
  python3 run_all.py                        # 运行全部 4 个 case
  python3 run_all.py FPGA-example1          # 只运行指定 case
  python3 run_all.py --parallel             # 并行运行
  python3 run_all.py --skip-env             # 跳过环境检测
  python3 run_all.py --json                 # 输出 JSON
  python3 run_all.py --help                 # 帮助信息
        """
    )
    parser.add_argument("case", nargs="?", default=None,
                        help="指定运行的 case 名称（默认运行全部）")
    parser.add_argument("--skip-env", action="store_true",
                        help="跳过环境检测")
    parser.add_argument("--parallel", action="store_true",
                        help="并行运行多个 case")
    parser.add_argument("--json", action="store_true",
                        help="输出 JSON 格式结果")

    args = parser.parse_args()

    global LOG_DIR
    timestamp = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
    LOG_DIR = SCRIPT_DIR / "logs" / timestamp
    LOG_DIR.mkdir(parents=True, exist_ok=True)

    print(f"{color('=' * 50, 'cyan')}")
    print(f"  VLSI FPGA 大作业 — 一键回归测试")
    print(f"  开始时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print(f"  项目目录: {PROJECT_DIR}")
    print(f"{color('=' * 50, 'cyan')}\n")

    # 阶段 1: 环境检测
    if not run_env_check(args.skip_env):
        sys.exit(2)

    # 阶段 2: 编译
    if not run_build():
        sys.exit(2)

    # 阶段 3: 运行
    header("阶段 3/5: 布局算法运行")

    cases_to_run = [args.case] if args.case else TEST_CASES
    results = []

    if args.parallel and len(cases_to_run) > 1:
        # 并行运行
        log(f"并行模式: 同时运行 {len(cases_to_run)} 个 case")
        with ThreadPoolExecutor(max_workers=min(len(cases_to_run), os.cpu_count() or 4)) as executor:
            future_map = {
                executor.submit(run_case, case, LOG_DIR): case
                for case in cases_to_run
            }
            for future in as_completed(future_map):
                results.append(future.result())
        # 按 case 名称排序
        results.sort(key=lambda r: cases_to_run.index(r["case"]))
    else:
        # 串行运行
        for case in cases_to_run:
            results.append(run_case(case, LOG_DIR))

    # 阶段 4: 汇总
    summary = print_summary(results, args.json)

    # 阶段 5: 快照
    write_memory_snapshot(results, summary)

    # 最终结果
    print()
    if summary["failed"] == 0:
        print(f"{color('=' * 50, 'green')}")
        print(f"{color('  ✅ 全部 {} 个 case 通过'.format(summary['total']), 'green')}")
        print(f"{color('=' * 50, 'green')}")
        sys.exit(0)
    else:
        msg = "  ⚠  {} / {} 个 case 失败".format(
            summary['failed'], summary['total'])
        print(f"{color('=' * 50, 'red')}")
        print(color(msg, 'red'))
        print(f"{color('=' * 50, 'red')}")
        sys.exit(1)


if __name__ == "__main__":
    main()
