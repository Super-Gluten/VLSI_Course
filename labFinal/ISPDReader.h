#ifndef ISPDREADER_H
#define ISPDREADER_H

#include <string>
#include <vector>
#include <map>
#include <set>
#include <fstream>
#include <sstream>
#include <iostream>
#include <memory>

#include "SiteType.h"

// ============================================================================
// 前向声明
// ============================================================================
class ISPDInstance;
class ISPDNet;
class ISPDSite;

// ============================================================================
// ISPDInstance — 对应 design.nodes + design.pl
// ============================================================================
class ISPDInstance {
public:
    std::string name;       // 如 "inst_2"
    std::string cell_type;  // 如 "RAMB36E2", "FDRE", "LUT6"
    int x = -1;             // x 坐标
    int y = -1;             // y 坐标
    int z = -1;             // z 坐标 (BEL index within site)
    bool is_fixed = false;
    std::vector<ISPDNet*> nets;  // 相连的 net

    ISPDInstance() = default;
    ISPDInstance(const std::string& n, const std::string& ct)
        : name(n), cell_type(ct) {}

    // 获取该 cell 属于哪个 resource 类型
    std::string getResourceType() const {
        return g_cell_resource_map.getResourceForCell(cell_type);
    }

    bool isLUT() const { return g_cell_resource_map.isLUT(cell_type); }
    bool isFF()  const { return g_cell_resource_map.isFF(cell_type); }
    int  getLUTSize() const { return g_cell_resource_map.getLUTSize(cell_type); }
    bool isFixed() const { return is_fixed; }
};

// ============================================================================
// ISPDNet — 对应 design.nets
// ============================================================================
class ISPDNet {
public:
    std::string name;  // 如 "net_1031"
    std::vector<std::pair<ISPDInstance*, std::string>> pins;  // (inst, port)

    ISPDNet() = default;
    ISPDNet(const std::string& n) : name(n) {}

    // 获取该 net 连接的所有 instance
    std::set<ISPDInstance*> getInstances() const {
        std::set<ISPDInstance*> insts;
        for (const auto& pin : pins) {
            insts.insert(pin.first);
        }
        return insts;
    }

    // 计算 HPWL
    int evalHPWL() const {
        if (pins.empty()) return 0;
        int min_x = 999999, min_y = 999999;
        int max_x = -999999, max_y = -999999;
        for (const auto& pin : pins) {
            auto* inst = pin.first;
            if (inst->x < min_x) min_x = inst->x;
            if (inst->y < min_y) min_y = inst->y;
            if (inst->x > max_x) max_x = inst->x;
            if (inst->y > max_y) max_y = inst->y;
        }
        return (max_x - min_x) + (max_y - min_y);
    }
};

// ============================================================================
// ISPDSite — FPGA 上的一个 Site (对应 design.scl 中的 SITEMAP)
// ============================================================================
class ISPDSite {
public:
    int x = 0;
    int y = 0;
    SiteType type = SiteType::UNKNOWN;

    // BEL 位置使用情况 (z 坐标)
    // SLICE: z 0-15 是 LUT, z 16-31 是 FF
    std::map<int, ISPDInstance*> bel_occupancy;

    ISPDSite() = default;
    ISPDSite(int sx, int sy, SiteType st)
        : x(sx), y(sy), type(st) {}

    // 检查该 site 是否能容纳指定 cell 类型
    bool canPlace(const std::string& cell_type) const {
        return g_cell_resource_map.isCellCompatibleWithSite(cell_type, type);
    }

    // 检查 z 坐标是否可用
    bool isBELFree(int z) const {
        return bel_occupancy.find(z) == bel_occupancy.end();
    }

    // 获取已占用的 BEL 数量
    int getUsedBELCount() const {
        return (int)bel_occupancy.size();
    }

    // 获取总 BEL 容量 (按 resource 类型)
    int getTotalBELCapacity(const std::string& resource) const {
        return g_site_capacity.getMaxCapacity(resource);
    }
};

// ============================================================================
// ISPDDatabase — 完整的数据集
// ============================================================================
class ISPDDatabase {
private:
    std::string base_dir_;

public:
    // 从 design.aux 读取的数据文件名
    std::string aux_file;
    std::string nodes_file;
    std::string nets_file;
    std::string wts_file;
    std::string pl_file;
    std::string scl_file;
    std::string lib_file;

    // 解析后的数据
    std::map<std::string, ISPDInstance*> instances;  // name -> instance
    std::map<std::string, ISPDNet*> nets;             // name -> net
    std::vector<std::vector<ISPDSite*>> site_map;     // [x][y] -> site
    int site_map_width = 0;
    int site_map_height = 0;

    // Cell 库信息 (来自 design.lib)
    std::map<std::string, int> cell_input_pin_count;  // cell_type -> input pin count

    ISPDDatabase() = default;
    ~ISPDDatabase();

    // 读取 benchmark 目录
    bool readBenchmark(const std::string& bench_dir);

    // 验证布局合法性
    int validatePlacement() const;

    // 计算总 HPWL
    int computeTotalHPWL() const;

    // 输出布局文件
    bool writePlacement(const std::string& output_file) const;

    // 获取可移动的 instance 列表
    std::vector<ISPDInstance*> getMovableInstances() const;

    // 获取空 site 列表 (按类型)
    std::vector<ISPDSite*> getEmptySites(SiteType type) const;

    // debug 信息
    void printStats() const;

private:
    bool readAuxFile(const std::string& path);
    bool readNodesFile(const std::string& path);
    bool readNetsFile(const std::string& path);
    bool readPlFile(const std::string& path);
    bool readSclFile(const std::string& path);
    bool readLibFile(const std::string& path);
};

#endif // ISPDREADER_H
