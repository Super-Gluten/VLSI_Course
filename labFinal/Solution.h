#ifndef SOLUTION_H
#define SOLUTION_H

#include <vector>
#include <set>
#include <map>
#include <random>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <iomanip>

#include "ISPDReader.h"
#include "SiteType.h"

// ============================================================================
// SA 布局求解器 — 支持 ISPD2016 资源约束 + 多实例/站点 (z坐标 BEL)
// ============================================================================
class Solution {
private:
    ISPDDatabase* db_;

    // 可移动 instance 列表
    std::vector<ISPDInstance*> movable_insts_;

    // 随机数引擎
    std::mt19937 rng_;
    std::uniform_real_distribution<double> uni_dist_{0.0, 1.0};

    // SA 统计
    int total_hpwl_ = 0;
    int best_hpwl_ = 0;
    int initial_hpwl_ = 0;
    int iter_count_ = 0;
    int accepted_count_ = 0;
    int rejected_legal_count_ = 0;

    // 为每种 SITE 类型缓存兼容的 cell 类型列表
    std::map<SiteType, std::vector<std::string>> site_compatible_cells_;

    // ============================================================
    // 核心函数
    // ============================================================

    // 检查将 inst 放到 site 是否合法 (资源兼容性)
    bool isLegalPlacement(const ISPDInstance* inst,
                          const ISPDSite* site) const;

    // 在指定 site 中找一个空闲的 BEL (z 坐标)，返回 -1 表示无空闲
    int findFreeBEL(const ISPDSite* site,
                    const ISPDInstance* inst) const;

    // 在 (cx, cy) 附近搜索兼容的 site + free BEL
    // radius 控制搜索半径，返回 true 表示找到
    bool searchNearbyBEL(int cx, int cy,
                         const ISPDInstance* inst,
                         int& out_x, int& out_y, int& out_z,
                         int max_radius = 10) const;

    // 初始布局：为所有 movable instance 分配 (x, y, z)
    // 每个站点可容纳多个实例（通过 BEL 打包）
    bool constructInitialPlacement();

    // 计算指定 net 集合的 HPWL
    int computeNetsHPWL(const std::set<ISPDNet*>& nets) const;

    // 计算所有 net 的总 HPWL
    int computeTotalHPWL() const;

    // 移动实例: 将 inst 从当前位置移到 target_site 的 target_z 位置
    void doMove(ISPDInstance* inst, ISPDSite* target_site, int target_z);

    // 力导向布局优化
    int forceDirectedPlacement();

public:
    Solution(ISPDDatabase* db)
        : db_(db)
        , rng_(std::chrono::steady_clock::now().time_since_epoch().count()) {
        initCompatibleCells();
    }

    // 主入口
    int solve();

    // 获取结果统计
    int getTotalHPWL() const { return total_hpwl_; }
    int getBestHPWL() const { return best_hpwl_; }
    int getInitialHPWL() const { return initial_hpwl_; }
    int getIterCount() const { return iter_count_; }
    int getAcceptedCount() const { return accepted_count_; }
    int getRejectedLegalCount() const { return rejected_legal_count_; }

private:
    // 初始化兼容 cell 类型缓存
    void initCompatibleCells();
};

#endif // SOLUTION_H
