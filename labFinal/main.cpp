#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>
#include <sys/stat.h>
#include <sys/types.h>

#include "ISPDReader.h"
#include "Solution.h"

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <benchmark_dir> <output_file>" << std::endl;
        std::cerr << "Example: " << argv[0] << " ../testbench/FPGA-example1 ./output/FPGA-example1_placement.txt" << std::endl;
        return -1;
    }

    std::string bench_dir(argv[1]);
    std::string output_file(argv[2]);

    auto start_time = std::chrono::steady_clock::now();

    // ================================================================
    // 阶段 1: 读取 benchmark
    // ================================================================
    std::cout << "==============================================" << std::endl;
    std::cout << "  VLSI FPGA 大作业 — ISPD2016 布局求解器" << std::endl;
    std::cout << "==============================================" << std::endl;
    std::cout << "  Benchmark: " << bench_dir << std::endl;
    std::cout << "  Output:    " << output_file << std::endl;
    std::cout << "==============================================" << std::endl;

    ISPDDatabase db;
    if (!db.readBenchmark(bench_dir)) {
        std::cerr << "[ERROR] Failed to read benchmark: " << bench_dir << std::endl;
        return -1;
    }

    db.printStats();

    // ================================================================
    // 阶段 2: 运行 SA 布局算法
    // ================================================================
    std::cout << std::endl;
    std::cout << "--- Running SA Placement ---" << std::endl;

    Solution solver(&db);
    int result = solver.solve();

    auto end_time = std::chrono::steady_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();

    if (result != 0) {
        std::cerr << "[ERROR] Placement algorithm failed" << std::endl;
        return -1;
    }

    // ================================================================
    // 阶段 4: 结果输出
    // ================================================================
    int final_hpwl = solver.getBestHPWL();
    int initial_hpwl = solver.getInitialHPWL();
    int errors = db.validatePlacement();

    std::cout << std::endl;
    std::cout << "==============================================" << std::endl;
    std::cout << "  布局结果" << std::endl;
    std::cout << "==============================================" << std::endl;
    std::cout << "  Best HPWL:         " << final_hpwl << std::endl;
    std::cout << "  Initial HPWL:      " << initial_hpwl << std::endl;
    std::cout << "  改善率:            ";
    if (initial_hpwl > 0) {
        double improvement = (1.0 - (double)final_hpwl / initial_hpwl) * 100.0;
        std::cout << std::fixed << std::setprecision(1) << improvement << "%" << std::endl;
    } else {
        std::cout << "N/A" << std::endl;
    }
    std::cout << "  布局错误:          " << errors << std::endl;
    std::cout << "  运行时间:          " << std::fixed << std::setprecision(1)
              << (elapsed_ms / 1000.0) << "s" << std::endl;

    // 输出到文件 (确保输出目录存在)
    {
        size_t pos = output_file.find_last_of('/');
        if (pos != std::string::npos) {
            std::string out_dir = output_file.substr(0, pos);
            mkdir(out_dir.c_str(), 0755);
        }
    }
    if (!db.writePlacement(output_file)) {
        std::cerr << "[ERROR] Failed to write output to: " << output_file << std::endl;
        return -1;
    }
    std::cout << "  输出文件:          " << output_file << std::endl;

    std::cout << "==============================================" << std::endl;

    return errors == 0 ? 0 : -1;
}
