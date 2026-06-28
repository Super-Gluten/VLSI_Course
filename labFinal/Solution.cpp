#include "Solution.h"

// ============================================================================
// 检查将 inst 放到 site 是否合法 (资源兼容性)
// ============================================================================
bool Solution::isLegalPlacement(const ISPDInstance* inst,
                                const ISPDSite* site) const {
    if (!site) return false;
    // 检查 site 类型是否与 cell 类型兼容
    return site->canPlace(inst->cell_type);
}

// ============================================================================
// 检查 BEL 约束 (简化版 Phase 1: 仅检查 z 是否可用)
// ============================================================================
bool Solution::isLegalBEL(const ISPDInstance* inst,
                          const ISPDSite* site,
                          int target_z) const {
    if (!site) return false;
    // 检查 z 是否在有效范围内 (0-15 for SLICE LUT/FF, 0 for others)
    if (site->type == SiteType::SLICE) {
        if (target_z < 0 || target_z >= 32) return false;

        // LUT6 只能放在奇数位，且 z-1 必须为空
        if (inst->isLUT() && inst->getLUTSize() == 6) {
            if (target_z % 2 == 0) return false;  // 必须奇数
            if (!site->isBELFree(target_z - 1)) return false;  // z-1 必须为空
        }
        // LUT5 两个共享时需相邻 (Phase 2 实现)
        // 其他 LUT: 总输入不超过 6 (Phase 2 实现)
    }

    // 检查 BEL 是否已被占用
    return site->isBELFree(target_z);
}

// ============================================================================
// 初始布局：为每个 movable instance 分配一个兼容的 site
// ============================================================================
bool Solution::constructInitialPlacement() {
    movable_insts_ = db_->getMovableInstances();
    int N = (int)movable_insts_.size();

    // 按 cell type 统计需要多少各种类型的 site
    std::map<SiteType, int> needed;
    for (auto* inst : movable_insts_) {
        std::string res = inst->getResourceType();
        SiteType st = SiteType::UNKNOWN;
        if (res == "LUT" || res == "FF" || res == "CARRY8") st = SiteType::SLICE;
        else if (res == "DSP48E2") st = SiteType::DSP;
        else if (res == "RAMB36E2") st = SiteType::BRAM;
        else if (res == "IO") st = SiteType::IO;
        needed[st]++;
    }

    // 收集每种类型的空 site
    free_sites_.clear();
    for (int x = 0; x < db_->site_map_width; x++) {
        for (int y = 0; y < db_->site_map_height; y++) {
            ISPDSite* site = db_->site_map[x][y];
            if (site && site->bel_occupancy.empty()) {
                free_sites_[site->type].push_back(site);
            }
        }
    }

    // 检查是否有足够的 site
    for (const auto& kv : needed) {
        size_t available = free_sites_[kv.first].size();
        if (available < (size_t)kv.second) {
            std::cerr << "[ERROR] Insufficient sites of type "
                      << siteTypeToString(kv.first)
                      << ": need " << kv.second
                      << ", have " << available << std::endl;
            return false;
        }
        // 打乱
        std::shuffle(free_sites_[kv.first].begin(),
                     free_sites_[kv.first].end(), rng_);
    }

    // 为每个 movable inst 分配 site
    occupied_sites_.resize(N);
    std::map<SiteType, size_t> used_count;

    for (int i = 0; i < N; i++) {
        ISPDInstance* inst = movable_insts_[i];
        std::string res = inst->getResourceType();

        SiteType st = SiteType::UNKNOWN;
        if (res == "LUT" || res == "FF" || res == "CARRY8") st = SiteType::SLICE;
        else if (res == "DSP48E2") st = SiteType::DSP;
        else if (res == "BRAM" || res == "RAMB36E2") st = SiteType::BRAM;
        else if (res == "IO") st = SiteType::IO;

        auto& site_list = free_sites_[st];
        if (used_count[st] >= site_list.size()) {
            std::cerr << "[ERROR] No free site for " << inst->name
                      << " (type=" << inst->cell_type << ")" << std::endl;
            return false;
        }

        ISPDSite* site = site_list[used_count[st]++];
        occupied_sites_[i] = site;

        // 更新 instance 坐标
        inst->x = site->x;
        inst->y = site->y;
        inst->z = 0;  // 简化处理

        // 更新 site 的 BEL 占用
        site->bel_occupancy[0] = inst;
    }

    // 从 free_sites_ 中移除已占用的
    for (auto& kv : free_sites_) {
        auto& list = kv.second;
        list.erase(list.begin(), list.begin() + used_count[kv.first]);
    }

    std::cout << "[INFO] Initial placement: " << N << " instances placed" << std::endl;
    return true;
}

// ============================================================================
// 增量 HPWL 计算
// ============================================================================
int Solution::computeNetsHPWL(const std::set<ISPDNet*>& nets) const {
    int total = 0;
    for (ISPDNet* net : nets) {
        total += net->evalHPWL();
    }
    return total;
}

int Solution::computeTotalHPWL() const {
    return db_->computeTotalHPWL();
}

// ============================================================================
// 执行移动
// ============================================================================
void Solution::doMove(ISPDInstance* inst,
                      ISPDSite* old_site,
                      ISPDSite* new_site) {
    // 从旧 site 移除
    for (auto it = old_site->bel_occupancy.begin();
         it != old_site->bel_occupancy.end(); ++it) {
        if (it->second == inst) {
            old_site->bel_occupancy.erase(it);
            break;
        }
    }

    // 放到新 site
    inst->x = new_site->x;
    inst->y = new_site->y;
    // 找一个空闲的 z 坐标
    int free_z = 0;
    while (!new_site->isBELFree(free_z)) free_z++;
    inst->z = free_z;
    new_site->bel_occupancy[free_z] = inst;
}

// ============================================================================
// SA 主求解器
// ============================================================================
int Solution::solve() {
    // ================================================================
    // 第一步：初始布局
    // ================================================================
    if (!constructInitialPlacement()) {
        std::cerr << "[ERROR] Initial placement failed" << std::endl;
        return -1;
    }

    int N = (int)movable_insts_.size();
    if (N == 0) {
        std::cout << "[INFO] No movable instances, skip placement." << std::endl;
        return 0;
    }

    // ================================================================
    // 第二步：SA 参数初始化
    // ================================================================
    total_hpwl_ = computeTotalHPWL();
    best_hpwl_ = total_hpwl_;
    initial_hpwl_ = total_hpwl_;

    // 自适应参数：根据问题规模调整
    // 对大规模问题(3337 inst, 22268 nets)，单次交换影响的 net 占比很小
    // 因此 delta 相对总 HPWL 很小，需要大幅降低 T0
    // 对大规模问题，使用更多迭代以获得更好的收敛
    double T0 = total_hpwl_ * 0.02;
    if (T0 < 1.0) T0 = 100.0;
    double alpha = 0.97;
    double T_min = T0 * std::pow(alpha, 300.0);
    int inner_loop = N * 15;

    std::cout << "[INFO] SA: N=" << N
              << ", T0=" << T0
              << ", alpha=" << alpha
              << ", inner_loop=" << inner_loop
              << ", initial_HPWL=" << total_hpwl_
              << std::endl;

    double T = T0;
    int last_best_report = 0;

    // ================================================================
    // 第三步：SA 主循环
    // ================================================================
    while (T > T_min) {
        int accepted = 0;
        int consecutive_reject = 0;
        int attempts = 0;

        for (int i = 0; i < inner_loop; i++) {
            // 策略：70% 概率使用 net-aware 交换，30% 纯随机
            int idx_a, idx_b;
            bool use_net_aware = (uni_dist_(rng_) < 0.7 && N > 10);

            if (use_net_aware) {
                // 选一个实例，然后从它相连的 net 中找另一个实例
                idx_a = std::uniform_int_distribution<int>(0, N - 1)(rng_);
                ISPDInstance* a_inst = movable_insts_[idx_a];
                if (!a_inst->nets.empty()) {
                    // 随机选一个 a 连接的 net
                    int net_idx = std::uniform_int_distribution<int>(
                        0, (int)a_inst->nets.size() - 1)(rng_);
                    ISPDNet* shared_net = a_inst->nets[net_idx];
                    // 从该 net 连接的实例中随机选一个作为 b
                    auto net_insts = shared_net->getInstances();
                    std::vector<ISPDInstance*> candidates;
                    for (auto* inst : net_insts) {
                        if (inst != a_inst && !inst->is_fixed) {
                            candidates.push_back(inst);
                        }
                    }
                    if (!candidates.empty()) {
                        int cidx = std::uniform_int_distribution<int>(
                            0, (int)candidates.size() - 1)(rng_);
                        ISPDInstance* b_inst = candidates[cidx];
                        // 找到 b 的索引
                        auto it = std::find(movable_insts_.begin(),
                                            movable_insts_.end(), b_inst);
                        if (it != movable_insts_.end()) {
                            idx_b = (int)(it - movable_insts_.begin());
                        } else {
                            use_net_aware = false;
                        }
                    } else {
                        use_net_aware = false;
                    }
                } else {
                    use_net_aware = false;
                }
            }

            if (!use_net_aware) {
                // 纯随机选择
                idx_a = std::uniform_int_distribution<int>(0, N - 1)(rng_);
                idx_b = std::uniform_int_distribution<int>(0, N - 1)(rng_);
            }

            if (idx_a == idx_b) continue;
            attempts++;

            ISPDInstance* a = movable_insts_[idx_a];
            ISPDInstance* b = movable_insts_[idx_b];
            ISPDSite* site_a = occupied_sites_[idx_a];
            ISPDSite* site_b = occupied_sites_[idx_b];

            // 资源兼容性检查：a 能否放到 site_b？b 能否放到 site_a？
            bool legal_a_to_b = isLegalPlacement(a, site_b);
            bool legal_b_to_a = isLegalPlacement(b, site_a);

            if (!legal_a_to_b || !legal_b_to_a) {
                rejected_legal_count_++;
                consecutive_reject++;
                if (consecutive_reject >= N * 50) break;
                continue;
            }

            // 收集受影响的 Net
            std::set<ISPDNet*> affected_nets;
            for (ISPDNet* net : a->nets) affected_nets.insert(net);
            for (ISPDNet* net : b->nets) affected_nets.insert(net);

            int hpwl_before = computeNetsHPWL(affected_nets);

            // 临时交换位置
            std::swap(a->x, b->x);
            std::swap(a->y, b->y);
            std::swap(a->z, b->z);

            int hpwl_after = computeNetsHPWL(affected_nets);
            int delta = hpwl_after - hpwl_before;

            bool accept = false;
            if (delta <= 0) {
                accept = true;
            } else {
                double prob = std::exp(-delta / T);
                if (uni_dist_(rng_) < prob) accept = true;
            }

            if (accept) {
                // 更新 site 的 BEL 占用
                // 从旧 site 移除 a 和 b
                for (auto it = site_a->bel_occupancy.begin();
                     it != site_a->bel_occupancy.end(); ) {
                    if (it->second == a || it->second == b) {
                        it = site_a->bel_occupancy.erase(it);
                    } else {
                        ++it;
                    }
                }
                for (auto it = site_b->bel_occupancy.begin();
                     it != site_b->bel_occupancy.end(); ) {
                    if (it->second == a || it->second == b) {
                        it = site_b->bel_occupancy.erase(it);
                    } else {
                        ++it;
                    }
                }

                // 放到新 site
                site_a->bel_occupancy[b->z] = b;  // b 现在在 a 的旧 site
                site_b->bel_occupancy[a->z] = a;  // a 现在在 b 的旧 site

                // 更新 occupied_sites_
                std::swap(occupied_sites_[idx_a], occupied_sites_[idx_b]);

                total_hpwl_ += delta;
                accepted++;
                consecutive_reject = 0;
                iter_count_++;

                if (total_hpwl_ < best_hpwl_) {
                    best_hpwl_ = total_hpwl_;
                    if (iter_count_ - last_best_report > 50000) {
                        std::cout << "[INFO] New best HPWL: " << best_hpwl_
                                  << " at iter=" << iter_count_
                                  << ", T=" << std::fixed
                                  << std::setprecision(1) << T
                                  << std::endl;
                        last_best_report = iter_count_;
                    }
                }
            } else {
                // 恢复位置
                std::swap(a->x, b->x);
                std::swap(a->y, b->y);
                std::swap(a->z, b->z);
                consecutive_reject++;
                iter_count_++;
            }
        }

        accepted_count_ += accepted;

        if (consecutive_reject >= N * 50) {
            std::cout << "[INFO] Early stop at T=" << T
                      << " (consecutive_reject=" << consecutive_reject << ")"
                      << std::endl;
            break;
        }

        T *= alpha;
    }

    // ================================================================
    // 第四步：结果报告
    // ================================================================
    std::cout << "[INFO] SA finished:" << std::endl;
    std::cout << "  Iterations: " << iter_count_ << std::endl;
    std::cout << "  Accepted: " << accepted_count_ << std::endl;
    std::cout << "  Rejected (legal): " << rejected_legal_count_ << std::endl;
    std::cout << "  Final HPWL: " << total_hpwl_ << std::endl;
    std::cout << "  Best HPWL: " << best_hpwl_ << std::endl;

    // 验证布局
    int errors = db_->validatePlacement();
    if (errors > 0) {
        std::cerr << "[WARN] Placement has " << errors << " errors" << std::endl;
    }

    return 0;
}
