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
// SA 布局求解器 — 支持 ISPD2016 资源约束
// ============================================================================
class Solution {
private:
    ISPDDatabase* db_;  // 持有数据库的指针

    // 可移动 instance 列表 (排除 fixed)
    std::vector<ISPDInstance*> movable_insts_;

    // 每个 movable instance 当前占用的 site 坐标
    // movable_insts_[i] 当前在 occupied_sites_[i]
    std::vector<ISPDSite*> occupied_sites_;

    // 空闲 site 列表 (按类型分组，加速查找)
    std::map<SiteType, std::vector<ISPDSite*>> free_sites_;

    // 随机数引擎
    std::mt19937 rng_;
    std::uniform_real_distribution<double> uni_dist_{0.0, 1.0};

    // SA 统计
    int total_hpwl_ = 0;
    int best_hpwl_ = 0;
    int initial_hpwl_ = 0;
    int iter_count_ = 0;
    int accepted_count_ = 0;
    int rejected_legal_count_ = 0;  // 因不合法被拒绝的次数

    // ============================================================
    // 核心函数
    // ============================================================

    // 检查将 inst 放到 site 是否合法 (资源兼容性)
    bool isLegalPlacement(const ISPDInstance* inst,
                          const ISPDSite* site) const;

    // 检查将 inst 放到 site 的 z 坐标是否合法 (BEL 约束)
    bool isLegalBEL(const ISPDInstance* inst,
                    const ISPDSite* site,
                    int target_z) const;

    // 初始布局：为每个 movable instance 分配一个合法的 site
    bool constructInitialPlacement();

    // 计算指定 net 集合的 HPWL (增量计算用)
    int computeNetsHPWL(const std::set<ISPDNet*>& nets) const;

    // 计算所有 net 的总 HPWL
    int computeTotalHPWL() const;

    // 执行一次交换: 将 inst 从 old_site 移到 new_site
    void doMove(ISPDInstance* inst, ISPDSite* old_site, ISPDSite* new_site);

public:
    Solution(ISPDDatabase* db)
        : db_(db)
        , rng_(std::chrono::steady_clock::now().time_since_epoch().count()) {}

    // 主入口：执行 SA 布局
    // 返回 0 表示成功
    int solve();

    // 获取结果统计
    int getTotalHPWL() const { return total_hpwl_; }
    int getBestHPWL() const { return best_hpwl_; }
    int getInitialHPWL() const { return initial_hpwl_; }
    int getIterCount() const { return iter_count_; }
    int getAcceptedCount() const { return accepted_count_; }
    int getRejectedLegalCount() const { return rejected_legal_count_; }
};

#endif // SOLUTION_H
