#pragma once
#ifndef SOLUTION_H
#define SOLUTION_H

#include <string>
#include <vector>
#include <set>
#include <random>
#include <algorithm>
#include "Global.h"
#include "Object.h"

class Solution
{
private:
    // 可移动 Instance 列表（排除 fixed）
    std::vector<Instance*> movable_insts;
    // 空闲 Block 坐标列表
    std::vector<std::pair<int, int>> empty_blocks;

    // Mersenne Twister 随机数引擎
    std::mt19937 rng;

    // 计算总 HPWL（遍历所有 Net）
    int computeTotalHPWL();
    // 计算指定 Net 集合的 HPWL（增量评估用）
    int computeNetsHPWL(const std::set<Net*>& nets);
    // 执行一次交换操作（含 Block-Instance 联动）
    void doSwap(Instance* a, Instance* b,
                int ax_old, int ay_old, int bx_old, int by_old);
    // 撤销一次交换操作
    void undoSave(Instance* a, Instance* b,
                  int ax_old, int ay_old, int bx_old, int by_old);

public:
    Solution();
    // 主入口：执行 SA 布局，返回 0 表示成功
    int solve();
};

int readBenchMarkFile(std::string i_file_name);

int outputSolution(std::string i_file_name);
// 报告当前布局的布线长度，使用HPWL线长预估模型
int reportWireLength();
// 报告当前布局是否合法
int reportValid();

#endif