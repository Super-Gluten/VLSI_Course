#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <cmath>

#include "Solution.h"
#include "Object.h"

// ==========================================================================
// Solution 类实现 - 模拟退火布局算法
// ==========================================================================

Solution::Solution()
    : rng(std::chrono::steady_clock::now().time_since_epoch().count())
{
}

int Solution::computeTotalHPWL()
{
    int total = 0;
    for (auto& kv : glb_net_map) {
        total += kv.second->evalHPWL();
    }
    return total;
}

int Solution::computeNetsHPWL(const std::set<Net*>& nets)
{
    int total = 0;
    for (Net* net : nets) {
        total += net->evalHPWL();
    }
    return total;
}

void Solution::doSwap(Instance* a, Instance* b,
                       int ax_old, int ay_old, int bx_old, int by_old)
{
    a->setPosition(bx_old, by_old);
    b->setPosition(ax_old, ay_old);

    glb_fpga.clearInst(ax_old, ay_old);
    glb_fpga.clearInst(bx_old, by_old);
    glb_fpga.addInst(ax_old, ay_old, b);
    glb_fpga.addInst(bx_old, by_old, a);
}

void Solution::undoSave(Instance* a, Instance* b,
                         int ax_old, int ay_old, int bx_old, int by_old)
{
    a->setPosition(ax_old, ay_old);
    b->setPosition(bx_old, by_old);

    glb_fpga.clearInst(ax_old, ay_old);
    glb_fpga.clearInst(bx_old, by_old);
    glb_fpga.addInst(ax_old, ay_old, a);
    glb_fpga.addInst(bx_old, by_old, b);
}

int Solution::solve()
{
    // ================================================================
    // 第一步：收集数据
    // ================================================================
    movable_insts.clear();
    empty_blocks.clear();

    for (auto& kv : glb_inst_map) {
        Instance* inst = kv.second;
        if (!inst->isFixed()) {
            movable_insts.push_back(inst);
        }
    }

    int size_x = glb_fpga.getSizeX();
    int size_y = glb_fpga.getSizeY();
    for (int x = 0; x < size_x; x++) {
        for (int y = 0; y < size_y; y++) {
            Block* block = glb_fpga.getBlock(x, y);
            if (block != nullptr && block->getInstsCount() == 0) {
                empty_blocks.push_back(std::make_pair(x, y));
            }
        }
    }

    int N = (int)movable_insts.size();
    int M = (int)empty_blocks.size();
    if (N > M) {
        std::printf("[ERROR] movable_insts(%d) > empty_blocks(%d)\n", N, M);
        return -1;
    }

    std::printf("[INFO] FPGA: %dx%d, Fixed: %zu, Movable: %d, "
                "Available blocks: %d, Nets: %zu\n",
                size_x, size_y,
                glb_inst_map.size() - N, N, M, glb_net_map.size());

    if (N == 0) {
        std::printf("[INFO] No movable instances, skip placement.\n");
        return 0;
    }

    // ================================================================
    // 第二步：随机初始布局
    //   将 empty_blocks 随机排列，前 N 个分配给 movable_insts，
    //   剩余 M-N 个作为空闲位置
    // ================================================================
    std::shuffle(empty_blocks.begin(), empty_blocks.end(), rng);

    // occupied_slots[i] 对应 movable_insts[i] 的当前坐标
    std::vector<std::pair<int, int>> occupied_slots(N);
    // free_slots: 当前空闲的 Block 坐标
    std::vector<std::pair<int, int>> free_slots;

    for (int i = 0; i < N; i++) {
        int x = empty_blocks[i].first;
        int y = empty_blocks[i].second;
        movable_insts[i]->setPosition(x, y);
        glb_fpga.addInst(x, y, movable_insts[i]);
        occupied_slots[i] = std::make_pair(x, y);
    }
    for (int i = N; i < M; i++) {
        free_slots.push_back(empty_blocks[i]);
    }

    int free_count = (int)free_slots.size();

    // ================================================================
    // 第三步：模拟退火主循环
    // ================================================================
    int current_cost = computeTotalHPWL();
    int best_cost = current_cost;

    // SA 参数
    double T0 = current_cost * 0.2;
    if (T0 < 1.0) T0 = 100.0;
    double alpha = 0.95;
    double T_min = T0 * std::pow(alpha, 150.0);
    int inner_loop = N * 10;

    std::printf("[INFO] SA: T0=%.1f, alpha=%.2f, inner_loop=%d, T_min=%.1f, "
                "free_slots=%d\n",
                T0, alpha, inner_loop, T_min, free_count);

    std::uniform_real_distribution<double> uni_dist(0.0, 1.0);
    std::uniform_int_distribution<int> idx_dist(0, N - 1);

    double T = T0;
    int iter_count = 0;
    int total_accepted = 0;
    int swap_count = 0;
    int move_count = 0;

    while (T > T_min) {
        int accepted = 0;
        int consecutive_reject = 0;

        for (int i = 0; i < inner_loop; i++) {
            // 扰动类型选择：70% 交换，30% 移动（如果有空闲位置）
            bool do_move = (free_count > 0) && (uni_dist(rng) < 0.3);

            if (do_move) {
                // ---- 移动扰动：将一个 Instance 移到空闲位置 ----
                int idx_a = idx_dist(rng);
                Instance* a = movable_insts[idx_a];
                int ax = occupied_slots[idx_a].first;
                int ay = occupied_slots[idx_a].second;

                // 随机选择一个空闲位置
                std::uniform_int_distribution<int> free_dist(0, free_count - 1);
                int free_idx = free_dist(rng);
                int fx = free_slots[free_idx].first;
                int fy = free_slots[free_idx].second;

                // 收集受影响的 Net
                std::set<Net*> affected_nets;
                for (Net* net : a->getNets()) affected_nets.insert(net);

                int hpwl_before = computeNetsHPWL(affected_nets);

                // 临时移动
                a->setPosition(fx, fy);
                int hpwl_after = computeNetsHPWL(affected_nets);
                int delta = hpwl_after - hpwl_before;

                bool accept = false;
                if (delta <= 0) {
                    accept = true;
                } else {
                    double prob = std::exp(-delta / T);
                    if (uni_dist(rng) < prob) accept = true;
                }

                if (accept) {
                    // 更新 FPGA Block
                    glb_fpga.clearInst(ax, ay);
                    glb_fpga.addInst(fx, fy, a);

                    // 更新 occupied_slots 和 free_slots
                    occupied_slots[idx_a] = std::make_pair(fx, fy);
                    free_slots[free_idx] = std::make_pair(ax, ay);

                    current_cost += delta;
                    accepted++;
                    move_count++;
                    consecutive_reject = 0;

                    if (current_cost < best_cost) {
                        best_cost = current_cost;
                        std::printf("[INFO] New best cost: %d at iter %d, T=%.1f\n",
                                    best_cost, iter_count, T);
                    }
                } else {
                    // 恢复位置
                    a->setPosition(ax, ay);
                    consecutive_reject++;
                }

            } else {
                // ---- 交换扰动：交换两个 Instance 位置 ----
                int idx_a = idx_dist(rng);
                int idx_b = idx_dist(rng);
                if (idx_a == idx_b) continue;

                Instance* a = movable_insts[idx_a];
                Instance* b = movable_insts[idx_b];
                int ax = occupied_slots[idx_a].first;
                int ay = occupied_slots[idx_a].second;
                int bx = occupied_slots[idx_b].first;
                int by = occupied_slots[idx_b].second;

                // 收集受影响的 Net
                std::set<Net*> affected_nets;
                for (Net* net : a->getNets()) affected_nets.insert(net);
                for (Net* net : b->getNets()) affected_nets.insert(net);

                int hpwl_before = computeNetsHPWL(affected_nets);

                // 临时交换
                a->setPosition(bx, by);
                b->setPosition(ax, ay);
                int hpwl_after = computeNetsHPWL(affected_nets);
                int delta = hpwl_after - hpwl_before;

                bool accept = false;
                if (delta <= 0) {
                    accept = true;
                } else {
                    double prob = std::exp(-delta / T);
                    if (uni_dist(rng) < prob) accept = true;
                }

                if (accept) {
                    // 更新 FPGA Block
                    glb_fpga.clearInst(ax, ay);
                    glb_fpga.clearInst(bx, by);
                    glb_fpga.addInst(ax, ay, b);
                    glb_fpga.addInst(bx, by, a);

                    // 更新 occupied_slots
                    occupied_slots[idx_a] = std::make_pair(bx, by);
                    occupied_slots[idx_b] = std::make_pair(ax, ay);

                    current_cost += delta;
                    accepted++;
                    swap_count++;
                    consecutive_reject = 0;

                    if (current_cost < best_cost) {
                        best_cost = current_cost;
                        std::printf("[INFO] New best cost: %d at iter %d, T=%.1f\n",
                                    best_cost, iter_count, T);
                    }
                } else {
                    a->setPosition(ax, ay);
                    b->setPosition(bx, by);
                    consecutive_reject++;
                }
            }

            iter_count++;

            if (consecutive_reject >= N * 50) break;
        }

        total_accepted += accepted;

        if (consecutive_reject >= N * 50) {
            std::printf("[INFO] Early stop at T=%.1f\n", T);
            break;
        }

        T *= alpha;
    }

    std::printf("[INFO] SA finished: %d iterations, %d accepted "
                "(swaps=%d, moves=%d), final cost=%d, best cost=%d\n",
                iter_count, total_accepted, swap_count, move_count,
                current_cost, best_cost);

    return 0;
}

// ==========================================================================
// 以下为原有函数，保持不变
// ==========================================================================

int readBenchMarkFile(std::string i_file_name){

    std::fstream f;
    f.open(i_file_name, std::ios::in);

    if (!f.is_open()){
        std::printf("file %s open failed", i_file_name.c_str());
        return -1;
    }

    std::string line;
    while (std::getline(f, line)){
        if (line.empty())
            break;
        
        std::istringstream iss(line);
        std::string temp;
        std::vector<std::string> row;
        while (iss >> temp){
            row.push_back(temp);
        }
        if (row.size() == 2){
            int l_size_x = std::stoi(row[0]);
            int l_size_y = std::stoi(row[1]);
            glb_fpga.setSize(l_size_x, l_size_y);
            glb_fpga.initialize();
        }else if (row.size() == 3){
            int l_inst_id, l_x, l_y;
            l_inst_id = std::stoi(row[0]);
            l_x = std::stoi(row[1]);
            l_y = std::stoi(row[2]);
            Instance* inst = new Instance(l_x, l_y, l_inst_id, true);
            glb_inst_map[l_inst_id] = inst;
            glb_fpga.addInst(l_x, l_y, inst);
        }else{
            std::printf("something wrong when try to parser: %s", line.c_str());
            return -1;
        }
    }
    while (std::getline(f, line)){
        if (line.empty())
            continue;
        
        std::istringstream iss(line);
        std::string temp;
        std::vector<std::string> row;
        while (iss >> temp){
            row.push_back(temp);
        }
        int l_inst_id, l_net_id;
        l_inst_id = std::stoi(row[0]);
        Instance* l_inst_point = nullptr;
        if (glb_inst_map.find(l_inst_id) == glb_inst_map.end()){
            l_inst_point = new Instance();
            l_inst_point->setInstId(l_inst_id);
            glb_inst_map[l_inst_id] = l_inst_point;
        }else{
            l_inst_point = glb_inst_map[l_inst_id];
        }
        
        for (size_t i = 1; i < row.size(); i++){
            l_net_id = std::stoi(row[i]);
            Net* lo_net_point = nullptr;
            if (glb_net_map.find(l_net_id) == glb_net_map.end()){
                lo_net_point = new Net;
                lo_net_point->setNetId(l_net_id);
                glb_net_map[l_net_id] = lo_net_point;
            }else{
                lo_net_point = glb_net_map[l_net_id];
            }
            l_inst_point->addNet(lo_net_point);
            lo_net_point->addInst(l_inst_point);
        }
    }
    f.close();
    return 0;
}

int outputSolution(std::string i_file_name){
    std::fstream f;
    f.open(i_file_name, std::ios::out);
    if (!f.is_open()){
        std::printf("unable to open file %s\n", i_file_name.c_str());
        return -1;
    }
    for (size_t i = 0; i < glb_inst_map.size(); i++){
        Instance* lo_inst_p = glb_inst_map[i];
        std::pair<int, int> lo_pos = lo_inst_p->getPosition();
        f << std::setw(5) << std::left << lo_inst_p->getInstId() \
            << std::setw(5) << std::left << lo_pos.first \
            << std::setw(5) << std::left << lo_pos.second << std::endl;
    }
    f.close();
    return 0;
}

int reportWireLength(){
    int l_wirelength = 0;
    for (auto lo_net : glb_net_map){
        l_wirelength += lo_net.second->evalHPWL();
    }
    std::cout << "Wirelength: " << std::setw(5) << std::right << l_wirelength << std::endl;
    return l_wirelength;
}

int reportValid(){
    int l_error_count = 0;
    for (auto lo_inst : glb_inst_map){
        Instance* lo_inst_p = lo_inst.second;
        std::pair<int, int> lo_inst_pos = lo_inst_p->getPosition();
        Block* lo_block_p = glb_fpga.getBlock(lo_inst_pos.first, lo_inst_pos.second);
        if (lo_block_p == nullptr){
            std::printf("[ERROR] inst %d is not placed (%d, %d)\n", lo_inst_p->getInstId(), lo_inst_pos.first, lo_inst_pos.second);
            l_error_count++;
            continue;
        }
        if (lo_block_p->getInsts()[0] != lo_inst_p){
            std::printf("[ERROR] inst %d is not in block (%d, %d)\n", lo_inst_p->getInstId(), lo_inst_pos.first, lo_inst_pos.second);
            l_error_count++;
        }
    }
    std::set<Instance*> lo_inst_attend;
    for (int i = 0; i < glb_fpga.getSizeX(); i++){
        for (int j = 0; j < glb_fpga.getSizeY(); j++){
            Block* lo_block_p = glb_fpga.getBlock(i, j);
            if (lo_block_p == nullptr)
                continue;
            for (auto lo_inst : lo_block_p->getInsts()){
                if (lo_inst_attend.find(lo_inst) != lo_inst_attend.end()){
                    std::printf("[ERROR] inst %d is repeated in block (%d, %d)\n", lo_inst->getInstId(), i, j);
                    l_error_count++; 
                } 
                lo_inst_attend.insert(lo_inst);
            }
        } 
    }
    return l_error_count;
}
