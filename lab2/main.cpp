#include <sstream>
#include <iostream>
#include <fstream>
#include <string>

#include "Global.h"
#include "Solution.h"

int main(int argv, char** argc){
    if (argv != 3){
        std::printf("usage: ./main benchmark.txt output.txt\n");
        return -2;
    }
    std::string l_input_file_name(argc[1]);
    int result = 0;

    result = readBenchMarkFile(l_input_file_name);
    if (result != 0){
        std::printf("read benchmark file failed\n");
        return -1;
    }
    Solution solve = Solution();
    // ========================================================
    // 调用模拟退火布局算法
    // ========================================================
    result = solve.solve();
    if (result != 0) {
        std::printf("placement algorithm failed\n");
        return -1;
    }

    std::string l_output_file_name(argc[2]);
    result = outputSolution(l_output_file_name);
    if (result != 0){
        std::printf("output solution failed\n");
        return -1;
    }
    glb_fpga.reportFPGA();
    reportWireLength();
    result = reportValid();
    for (auto lo_inst : glb_inst_map)
        delete lo_inst.second;
    for (auto lo_net : glb_net_map)
        delete lo_net.second;
    glb_inst_map.clear();
    glb_net_map.clear();
    std::printf("[INFO] Program exited with: %d errors\n", result);
    return result == 0 ? 0 : -1;
}