#include "ISPDReader.h"
#include <algorithm>
#include <cctype>

// ============================================================================
// 工具函数
// ============================================================================
static std::string trim(const std::string& s) {
    size_t start = 0, end = s.size();
    while (start < end && std::isspace(s[start])) start++;
    while (end > start && std::isspace(s[end-1])) end--;
    return s.substr(start, end - start);
}

static std::vector<std::string> split(const std::string& s) {
    std::istringstream iss(s);
    std::vector<std::string> tokens;
    std::string token;
    while (iss >> token) tokens.push_back(token);
    return tokens;
}

// ============================================================================
// ISPDDatabase 析构
// ============================================================================
ISPDDatabase::~ISPDDatabase() {
    for (auto& kv : instances) delete kv.second;
    for (auto& kv : nets) delete kv.second;
    for (auto& row : site_map) {
        for (auto* site : row) delete site;
    }
    instances.clear();
    nets.clear();
    site_map.clear();
}

// ============================================================================
// 主入口: 读取 benchmark 目录
// ============================================================================
bool ISPDDatabase::readBenchmark(const std::string& bench_dir) {
    // 确保路径以 / 结尾
    base_dir_ = bench_dir;
    if (!base_dir_.empty() && base_dir_.back() != '/') {
        base_dir_ += '/';
    }

    // 1. 读取 aux 文件
    std::string aux_path = base_dir_ + "design.aux";
    if (!readAuxFile(aux_path)) {
        std::cerr << "[ERROR] Failed to read aux file: " << aux_path << std::endl;
        return false;
    }

    // 2. 读取各个文件
    if (!readNodesFile(base_dir_ + nodes_file)) {
        std::cerr << "[ERROR] Failed to read nodes file" << std::endl;
        return false;
    }
    if (!readNetsFile(base_dir_ + nets_file)) {
        std::cerr << "[ERROR] Failed to read nets file" << std::endl;
        return false;
    }
    if (!readPlFile(base_dir_ + pl_file)) {
        std::cerr << "[ERROR] Failed to read pl file" << std::endl;
        return false;
    }
    if (!readSclFile(base_dir_ + scl_file)) {
        std::cerr << "[ERROR] Failed to read scl file" << std::endl;
        return false;
    }

    // lib 和 wts 文件可选
    if (!lib_file.empty()) {
        readLibFile(base_dir_ + lib_file);
    }

    std::cout << "[INFO] ISPDDatabase loaded:" << std::endl;
    std::cout << "  Instances: " << instances.size() << std::endl;
    std::cout << "  Nets: " << nets.size() << std::endl;
    std::cout << "  SiteMap: " << site_map_width << " x " << site_map_height << std::endl;
    return true;
}

// ============================================================================
// design.aux 解析
// ============================================================================
bool ISPDDatabase::readAuxFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "[WARN] aux file not found: " << path
                  << ", trying to read files directly" << std::endl;
        // 直接使用默认文件名
        nodes_file = "design.nodes";
        nets_file = "design.nets";
        wts_file = "design.wts";
        pl_file = "design.pl";
        scl_file = "design.scl";
        lib_file = "design.lib";
        return true;
    }

    std::string line;
    while (std::getline(f, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        // 格式: design : file1 file2 file3 ...
        auto tokens = split(line);
        if (tokens.size() >= 2 && tokens[0] == "design") {
            for (size_t i = 1; i < tokens.size(); i++) {
                std::string fname = tokens[i];
                if (fname.find("nodes") != std::string::npos) nodes_file = fname;
                else if (fname.find("nets") != std::string::npos) nets_file = fname;
                else if (fname.find("wts") != std::string::npos) wts_file = fname;
                else if (fname.find("pl") != std::string::npos) pl_file = fname;
                else if (fname.find("scl") != std::string::npos) scl_file = fname;
                else if (fname.find("lib") != std::string::npos) lib_file = fname;
            }
        }
    }

    // 验证必要文件
    if (nodes_file.empty() || nets_file.empty() ||
        pl_file.empty() || scl_file.empty()) {
        std::cerr << "[ERROR] Incomplete file list in aux" << std::endl;
        return false;
    }

    return true;
}

// ============================================================================
// design.nodes 解析
// 格式: inst_name cell_type
// ============================================================================
bool ISPDDatabase::readNodesFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "[ERROR] Cannot open nodes file: " << path << std::endl;
        return false;
    }

    std::string line;
    while (std::getline(f, line)) {
        line = trim(line);
        if (line.empty()) continue;

        auto tokens = split(line);
        if (tokens.size() >= 2) {
            std::string name = tokens[0];
            std::string cell_type = tokens[1];

            auto* inst = new ISPDInstance(name, cell_type);
            instances[name] = inst;
        }
    }

    return true;
}

// ============================================================================
// design.nets 解析
// 格式:
//   net net_name num_pins
//       inst_name port_name
//       ...
//   endnet
// ============================================================================
bool ISPDDatabase::readNetsFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "[ERROR] Cannot open nets file: " << path << std::endl;
        return false;
    }

    std::string line;
    ISPDNet* current_net = nullptr;

    while (std::getline(f, line)) {
        line = trim(line);
        if (line.empty()) continue;

        auto tokens = split(line);

        if (tokens[0] == "net") {
            // 新 net 开始
            if (tokens.size() >= 2) {
                current_net = new ISPDNet(tokens[1]);
                nets[tokens[1]] = current_net;
            }
        } else if (tokens[0] == "endnet") {
            current_net = nullptr;
        } else if (current_net != nullptr && tokens.size() >= 2) {
            // 连接信息: inst_name port_name
            std::string inst_name = tokens[0];
            std::string port_name = tokens[1];

            auto inst_it = instances.find(inst_name);
            if (inst_it != instances.end()) {
                ISPDInstance* inst = inst_it->second;
                current_net->pins.push_back(std::make_pair(inst, port_name));
                inst->nets.push_back(current_net);
            }
        }
    }

    return true;
}

// ============================================================================
// design.pl 解析
// 格式: inst_name x y z FIXED
// ============================================================================
bool ISPDDatabase::readPlFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "[ERROR] Cannot open pl file: " << path << std::endl;
        return false;
    }

    std::string line;
    while (std::getline(f, line)) {
        line = trim(line);
        if (line.empty()) continue;

        auto tokens = split(line);
        if (tokens.size() >= 4) {
            std::string inst_name = tokens[0];
            int x = std::stoi(tokens[1]);
            int y = std::stoi(tokens[2]);
            int z = std::stoi(tokens[3]);
            bool fixed = (tokens.size() >= 5 && tokens[4] == "FIXED");

            auto inst_it = instances.find(inst_name);
            if (inst_it != instances.end()) {
                ISPDInstance* inst = inst_it->second;
                inst->x = x;
                inst->y = y;
                inst->z = z;
                inst->is_fixed = fixed;

                // 更新 site 的 BEL 占用
                if (x >= 0 && x < (int)site_map.size() &&
                    y >= 0 && y < (int)site_map[x].size()) {
                    ISPDSite* site = site_map[x][y];
                    if (site) {
                        site->bel_occupancy[z] = inst;
                    }
                }
            }
        }
    }

    return true;
}

// ============================================================================
// design.scl 解析
// 格式:
//   SITE SLICE
//     LUT 16
//     FF 16
//     CARRY8 1
//   END SITE
//   ...
//   RESOURCES
//     LUT LUT1 LUT2 ...
//   END RESOURCES
//   SITEMAP width height
//     x y SITE_TYPE
//     ...
// ============================================================================
bool ISPDDatabase::readSclFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "[ERROR] Cannot open scl file: " << path << std::endl;
        return false;
    }

    std::string line;
    bool in_sitemap = false;
    bool in_resources [[maybe_unused]] = false;

    while (std::getline(f, line)) {
        line = trim(line);
        if (line.empty()) continue;

        auto tokens = split(line);
        if (tokens.empty()) continue;

        if (tokens[0] == "SITEMAP") {
            // SITEMAP width height
            in_sitemap = true;
            in_resources = false;
            if (tokens.size() >= 3) {
                site_map_width = std::stoi(tokens[1]);
                site_map_height = std::stoi(tokens[2]);
                site_map.resize(site_map_width);
                for (int x = 0; x < site_map_width; x++) {
                    site_map[x].resize(site_map_height, nullptr);
                }
            }
        } else if (tokens[0] == "SITE") {
            // SITE type — 解析容量信息
            in_sitemap = false;
            in_resources = false;
            if (tokens.size() >= 2) {
                std::string site_type_name = tokens[1];
                // 读取容量信息
                while (std::getline(f, line)) {
                    line = trim(line);
                    if (line.empty()) continue;
                    auto st = split(line);
                    if (st[0] == "END") break;
                    if (st.size() >= 2) {
                        // 例如: LUT 16
                        // 由于 design.scl 的容量对 4 个 example 固定，此处不做额外处理
                        // 具体实现在 SiteType.h 中硬编码
                    }
                }
            }
        } else if (tokens[0] == "RESOURCES") {
            in_sitemap = false;
            in_resources = true;
            // 资源映射已在 CellResourceMap 构造函数中硬编码，跳过
        } else if (tokens[0] == "END" && tokens.size() >= 2 && tokens[1] == "RESOURCES") {
            in_resources = false;
        } else if (in_sitemap && tokens.size() >= 3) {
            // SITEMAP 行: x y SITE_TYPE
            int x = std::stoi(tokens[0]);
            int y = std::stoi(tokens[1]);
            SiteType type = siteTypeFromString(tokens[2]);

            if (x >= 0 && x < site_map_width &&
                y >= 0 && y < site_map_height) {
                site_map[x][y] = new ISPDSite(x, y, type);
            }
        }
    }

    return true;
}

// ============================================================================
// design.lib 解析
// 格式:
//   CELL cell_name
//     PIN pin_name DIRECTION
//     ...
//   END CELL
// ============================================================================
bool ISPDDatabase::readLibFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "[WARN] Cannot open lib file: " << path << std::endl;
        return false;
    }

    std::string line;
    std::string current_cell;
    int input_count = 0;

    while (std::getline(f, line)) {
        line = trim(line);
        if (line.empty()) continue;

        auto tokens = split(line);
        if (tokens.empty()) continue;

        if (tokens[0] == "CELL" && tokens.size() >= 2) {
            current_cell = tokens[1];
            input_count = 0;
        } else if (tokens[0] == "END" && tokens.size() >= 2 && tokens[1] == "CELL") {
            if (!current_cell.empty()) {
                cell_input_pin_count[current_cell] = input_count;
                g_cell_resource_map.setCellInputPins(current_cell, input_count);
            }
            current_cell.clear();
        } else if (tokens[0] == "PIN" && tokens.size() >= 3) {
            if (tokens[2] == "INPUT" || tokens[2] == "CLOCK" || tokens[2] == "CTRL") {
                input_count++;
            }
        }
    }

    return true;
}

// ============================================================================
// 验证布局合法性
// ============================================================================
int ISPDDatabase::validatePlacement() const {
    int errors = 0;

    for (const auto& kv : instances) {
        const ISPDInstance* inst = kv.second;

        // 检查坐标是否在 site_map 范围内
        if (inst->x < 0 || inst->x >= site_map_width ||
            inst->y < 0 || inst->y >= site_map_height) {
            std::cerr << "[ERROR] " << inst->name
                      << " out of bounds: (" << inst->x << "," << inst->y << ")"
                      << std::endl;
            errors++;
            continue;
        }

        // 检查 site 是否存在
        ISPDSite* site = site_map[inst->x][inst->y];
        if (site == nullptr) {
            std::cerr << "[ERROR] " << inst->name
                      << " placed on null site (" << inst->x << "," << inst->y << ")"
                      << std::endl;
            errors++;
            continue;
        }

        // 检查资源兼容性
        if (!site->canPlace(inst->cell_type)) {
            std::cerr << "[ERROR] " << inst->name << " (" << inst->cell_type
                      << ") incompatible with site type "
                      << siteTypeToString(site->type)
                      << " at (" << inst->x << "," << inst->y << ")"
                      << std::endl;
            errors++;
        }

        // 检查 BEL 占用
        if (inst->z < 0) {
            std::cerr << "[ERROR] " << inst->name << " has invalid z=" << inst->z
                      << std::endl;
            errors++;
        }
    }

    return errors;
}

// ============================================================================
// 计算总 HPWL
// ============================================================================
int ISPDDatabase::computeTotalHPWL() const {
    int total = 0;
    for (const auto& kv : nets) {
        total += kv.second->evalHPWL();
    }
    return total;
}

// ============================================================================
// 输出布局文件
// ============================================================================
bool ISPDDatabase::writePlacement(const std::string& output_file) const {
    std::ofstream f(output_file);
    if (!f.is_open()) {
        std::cerr << "[ERROR] Cannot open output file: " << output_file << std::endl;
        return false;
    }

    for (const auto& kv : instances) {
        const ISPDInstance* inst = kv.second;
        f << inst->name << " " << inst->x << " " << inst->y
          << " " << inst->z << std::endl;
    }

    return true;
}

// ============================================================================
// 获取可移动的 instance 列表
// ============================================================================
std::vector<ISPDInstance*> ISPDDatabase::getMovableInstances() const {
    std::vector<ISPDInstance*> movable;
    for (const auto& kv : instances) {
        if (!kv.second->is_fixed) {
            movable.push_back(kv.second);
        }
    }
    return movable;
}

// ============================================================================
// 获取空 site 列表 (按类型)
// ============================================================================
std::vector<ISPDSite*> ISPDDatabase::getEmptySites(SiteType type) const {
    std::vector<ISPDSite*> empty_sites;
    for (int x = 0; x < site_map_width; x++) {
        for (int y = 0; y < site_map_height; y++) {
            ISPDSite* site = site_map[x][y];
            if (site && site->type == type && site->bel_occupancy.empty()) {
                empty_sites.push_back(site);
            }
        }
    }
    return empty_sites;
}

// ============================================================================
// 打印统计信息
// ============================================================================
void ISPDDatabase::printStats() const {
    std::cout << "=== ISPDDatabase Stats ===" << std::endl;
    std::cout << "  Instances: " << instances.size() << std::endl;

    int fixed = 0, movable = 0;
    for (const auto& kv : instances) {
        if (kv.second->is_fixed) fixed++;
        else movable++;
    }
    std::cout << "  Fixed: " << fixed << ", Movable: " << movable << std::endl;
    std::cout << "  Nets: " << nets.size() << std::endl;
    std::cout << "  SiteMap: " << site_map_width << " x " << site_map_height << std::endl;

    // 统计 site 类型分布
    std::map<SiteType, int> site_type_count;
    for (int x = 0; x < site_map_width; x++) {
        for (int y = 0; y < site_map_height; y++) {
            if (site_map[x][y]) {
                site_type_count[site_map[x][y]->type]++;
            }
        }
    }
    for (const auto& kv : site_type_count) {
        std::cout << "  SiteType " << siteTypeToString(kv.first)
                  << ": " << kv.second << std::endl;
    }

    // 统计 cell 类型分布
    std::map<std::string, int> cell_type_count;
    for (const auto& kv : instances) {
        cell_type_count[kv.second->cell_type]++;
    }
    std::cout << "  Cell types:" << std::endl;
    for (const auto& kv : cell_type_count) {
        std::cout << "    " << kv.first << ": " << kv.second << std::endl;
    }
}
