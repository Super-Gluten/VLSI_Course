#ifndef SITETYPE_H
#define SITETYPE_H

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstring>

// ============================================================================
// FPGA Site 类型枚举
// ============================================================================
enum class SiteType : int {
    SLICE = 0,
    DSP   = 1,
    BRAM  = 2,
    IO    = 3,
    UNKNOWN = 4
};

// Site 类型名称 -> 枚举映射
inline SiteType siteTypeFromString(const std::string& s) {
    if (s == "SLICE") return SiteType::SLICE;
    if (s == "DSP")   return SiteType::DSP;
    if (s == "BRAM")  return SiteType::BRAM;
    if (s == "IO")    return SiteType::IO;
    return SiteType::UNKNOWN;
}

inline std::string siteTypeToString(SiteType t) {
    switch (t) {
        case SiteType::SLICE: return "SLICE";
        case SiteType::DSP:   return "DSP";
        case SiteType::BRAM:  return "BRAM";
        case SiteType::IO:    return "IO";
        default:              return "UNKNOWN";
    }
}

// ============================================================================
// Cell (电路元件) 类型 -> 资源类型 映射关系
// 对应 design.scl 中的 RESOURCES 段
// ============================================================================
class CellResourceMap {
private:
    // resource_type -> set of cell_type_names
    std::unordered_map<std::string, std::unordered_set<std::string>> resource_to_cells;
    // cell_type_name -> resource_type
    std::unordered_map<std::string, std::string> cell_to_resource;
    // cell_type_name -> number of input pins (from .lib)
    std::unordered_map<std::string, int> cell_input_pins;

public:
    CellResourceMap() {
        // 硬编码已知的资源映射 (design.scl 固定不变)
        addMapping("LUT", {
            "LUT1", "LUT2", "LUT3", "LUT4", "LUT5", "LUT6"
        });
        addMapping("FF", { "FDRE" });
        addMapping("CARRY8", { "CARRY8" });
        addMapping("DSP48E2", { "DSP48E2" });
        addMapping("RAMB36E2", { "RAMB36E2" });
        addMapping("IO", { "IBUF", "OBUF", "BUFGCE" });
    }

    void addMapping(const std::string& resource,
                    const std::unordered_set<std::string>& cells) {
        for (const auto& cell : cells) {
            resource_to_cells[resource].insert(cell);
            cell_to_resource[cell] = resource;
        }
    }

    // 获取 cell 类型对应的 resource 类型
    std::string getResourceForCell(const std::string& cell_type) const {
        auto it = cell_to_resource.find(cell_type);
        if (it != cell_to_resource.end()) return it->second;
        return "";
    }

    // 判断 cell 类型是否属于某个 resource
    bool isCellInResource(const std::string& cell_type,
                          const std::string& resource) const {
        auto it = resource_to_cells.find(resource);
        if (it == resource_to_cells.end()) return false;
        return it->second.count(cell_type) > 0;
    }

    // 判断 cell 类型是否能放在指定 Site 类型上
    bool isCellCompatibleWithSite(const std::string& cell_type,
                                  SiteType site_type) const {
        std::string res = getResourceForCell(cell_type);
        if (res.empty()) return false;

        switch (site_type) {
            case SiteType::SLICE:
                return (res == "LUT" || res == "FF" || res == "CARRY8");
            case SiteType::DSP:
                return (res == "DSP48E2");
            case SiteType::BRAM:
                return (res == "RAMB36E2");
            case SiteType::IO:
                return (res == "IO");
            default:
                return false;
        }
    }

    // 设置 cell 的输入引脚数量 (从 design.lib 解析)
    void setCellInputPins(const std::string& cell_type, int num_pins) {
        cell_input_pins[cell_type] = num_pins;
    }

    int getCellInputPins(const std::string& cell_type) const {
        auto it = cell_input_pins.find(cell_type);
        if (it != cell_input_pins.end()) return it->second;
        return 0;
    }

    // 判断 cell 是否为 LUT 类型
    bool isLUT(const std::string& cell_type) const {
        return resource_to_cells.at("LUT").count(cell_type) > 0;
    }

    // 判断 cell 是否为 FF 类型
    bool isFF(const std::string& cell_type) const {
        return resource_to_cells.at("FF").count(cell_type) > 0;
    }

    // 从 LUT 类型名获取 LUT 大小 (如 LUT6 -> 6)
    int getLUTSize(const std::string& cell_type) const {
        if (cell_type.size() >= 4 && cell_type.substr(0, 3) == "LUT") {
            return std::stoi(cell_type.substr(3));
        }
        return 0;
    }
};

// ============================================================================
// Site 资源容量定义 (对应 design.scl 的 SITE 定义段)
// ============================================================================
struct SiteCapacity {
    int lut_capacity = 16;
    int ff_capacity = 16;
    int carry8_capacity = 1;
    int dsp_capacity = 1;
    int bram_capacity = 1;
    int io_capacity = 64;

    int getMaxCapacity(const std::string& resource) const {
        if (resource == "LUT")    return lut_capacity;
        if (resource == "FF")     return ff_capacity;
        if (resource == "CARRY8") return carry8_capacity;
        if (resource == "DSP48E2") return dsp_capacity;
        if (resource == "RAMB36E2") return bram_capacity;
        if (resource == "IO")     return io_capacity;
        return 0;
    }
};

// ============================================================================
// 全局单例
// ============================================================================
extern CellResourceMap g_cell_resource_map;
extern SiteCapacity g_site_capacity;

#endif // SITETYPE_H
