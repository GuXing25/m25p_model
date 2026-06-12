#include "config_parser.h"
#include <fstream>
#include <iostream>
#include <sstream>

using namespace std;

static bool is_power_of_two(int value) {
    return value > 0 && (value & (value - 1)) == 0;
}

// 默认参数直接对应 M25P40 的常用规格：
// 8 个 64KiB sector，每个 sector 256 页，每页 256B，总容量 512KiB。
// 即使配置文件缺失，模型也能使用这组默认值运行。
FlashConfig::FlashConfig()
    : page_size(256),
      page_per_sector(256),
      sector_count(8),
      total_pages(2048),
      memory_size(524288),
      f_c_mhz(75.0),
      f_r_mhz(33.0),
      t_shsl_us(0.1),
      t_read_us(0.0),
      t_fast_read_us(0.0),
      t_w_us(1300.0),
      t_w_max_us(15000.0),
      t_prog_us(800.0),
      t_prog_max_us(5000.0),
      t_prog_chunk_us(25.0),
      t_erase_sector_us(600000.0),
      t_erase_sector_max_us(3000000.0),
      t_erase_bulk_us(4500000.0),
      t_erase_bulk_max_us(10000000.0),
      t_dpd_us(3.0),
      t_res_us(30.0),
      t_vsl_us(10.0),
      t_puw_us(10000.0),
      use_max_time(0),
      auto_complete(0),
      wrap_address(1),
      storage_file("storage_m25p.bin") {}

FlashConfig load_config(const std::string& filename) {
    FlashConfig cfg;
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "[警告] 配置文件打开失败，使用 M25P40 默认参数！" << endl;
        return cfg;
    }

    string line, key, str_val;
    double val;
    while (getline(file, line)) {
        // 配置文件采用简单的 "KEY VALUE" 格式；空行和 # 注释行跳过。
        if (line.empty() || line[0] == '#') continue;
        stringstream ss(line);
        ss >> key >> str_val;
        if (key.empty() || str_val.empty()) continue;

        // 大多数字段是数字，STORAGE_FILE 是唯一的字符串字段。
        // 这里先统一尝试按 double 解析，后面按字段类型再转 int/double。
        stringstream vs(str_val);
        vs >> val;

        // 几何参数：允许配置文件显式给出，但最终仍会按
        // sector_count * page_per_sector * page_size 重新归一化。
        if (key == "PAGE_SIZE") cfg.page_size = (int)val;
        else if (key == "PAGE_PER_SECTOR") cfg.page_per_sector = (int)val;
        else if (key == "SECTOR_COUNT") cfg.sector_count = (int)val;
        else if (key == "TOTAL_PAGES") cfg.total_pages = (int)val;
        else if (key == "MEMORY_SIZE") cfg.memory_size = (int)val;

        // SPI 频率和片选间隔。
        else if (key == "F_C_MHZ") cfg.f_c_mhz = val;
        else if (key == "F_R_MHZ") cfg.f_r_mhz = val;
        else if (key == "T_SHSL_US") cfg.t_shsl_us = val;

        // 命令内部周期，单位 us。
        else if (key == "T_READ_US") cfg.t_read_us = val;
        else if (key == "T_FAST_READ_US") cfg.t_fast_read_us = val;
        else if (key == "T_W_US") cfg.t_w_us = val;
        else if (key == "T_W_MAX_US") cfg.t_w_max_us = val;
        else if (key == "T_PROG_US") cfg.t_prog_us = val;
        else if (key == "T_PROG_MAX_US") cfg.t_prog_max_us = val;
        else if (key == "T_PROG_CHUNK_US") cfg.t_prog_chunk_us = val;
        else if (key == "T_ERASE_SECTOR_US") cfg.t_erase_sector_us = val;
        else if (key == "T_ERASE_SECTOR_MAX_US") cfg.t_erase_sector_max_us = val;
        else if (key == "T_ERASE_BULK_US") cfg.t_erase_bulk_us = val;
        else if (key == "T_ERASE_BULK_MAX_US") cfg.t_erase_bulk_max_us = val;
        else if (key == "T_DPD_US") cfg.t_dpd_us = val;
        else if (key == "T_RES_US") cfg.t_res_us = val;
        else if (key == "T_VSL_US") cfg.t_vsl_us = val;
        else if (key == "T_PUW_US") cfg.t_puw_us = val;

        // 仿真策略开关。
        else if (key == "USE_MAX_TIME") cfg.use_max_time = (int)val;
        else if (key == "AUTO_COMPLETE") cfg.auto_complete = (int)val;
        else if (key == "WRAP_ADDRESS") cfg.wrap_address = (int)val;
        else if (key == "STORAGE_FILE") cfg.storage_file = str_val;
    }

    if (!is_power_of_two(cfg.page_size)) {
        cerr << "[警告] PAGE_SIZE 必须为正的 2 的幂，恢复默认值 256" << endl;
        cfg.page_size = 256;
    }
    if (cfg.page_per_sector <= 0) {
        cerr << "[警告] PAGE_PER_SECTOR 非法，恢复默认值 256" << endl;
        cfg.page_per_sector = 256;
    }
    if (cfg.sector_count <= 0) {
        cerr << "[警告] SECTOR_COUNT 非法，恢复默认值 8" << endl;
        cfg.sector_count = 8;
    }
    if (cfg.f_c_mhz <= 0.0) {
        cerr << "[警告] F_C_MHZ 非法，恢复默认值 75MHz" << endl;
        cfg.f_c_mhz = 75.0;
    }
    if (cfg.f_r_mhz <= 0.0) {
        cerr << "[警告] F_R_MHZ 非法，恢复默认值 33MHz" << endl;
        cfg.f_r_mhz = 33.0;
    }
    if (cfg.t_prog_us < 0.0) cfg.t_prog_us = 0.0;
    if (cfg.t_prog_max_us < cfg.t_prog_us) cfg.t_prog_max_us = cfg.t_prog_us;
    if (cfg.t_prog_chunk_us <= 0.0) cfg.t_prog_chunk_us = 25.0;
    if (cfg.t_erase_sector_us < 0.0) cfg.t_erase_sector_us = 0.0;
    if (cfg.t_erase_sector_max_us < cfg.t_erase_sector_us) {
        cfg.t_erase_sector_max_us = cfg.t_erase_sector_us;
    }
    if (cfg.t_erase_bulk_us < 0.0) cfg.t_erase_bulk_us = 0.0;
    if (cfg.t_erase_bulk_max_us < cfg.t_erase_bulk_us) {
        cfg.t_erase_bulk_max_us = cfg.t_erase_bulk_us;
    }
    if (cfg.t_w_us < 0.0) cfg.t_w_us = 0.0;
    if (cfg.t_w_max_us < cfg.t_w_us) cfg.t_w_max_us = cfg.t_w_us;
    if (cfg.t_dpd_us < 0.0) cfg.t_dpd_us = 0.0;
    if (cfg.t_res_us < 0.0) cfg.t_res_us = 0.0;
    if (cfg.t_shsl_us < 0.0) cfg.t_shsl_us = 0.0;
    if (cfg.t_vsl_us < 0.0) cfg.t_vsl_us = 0.0;
    if (cfg.t_puw_us < 0.0) cfg.t_puw_us = 0.0;
    cfg.use_max_time = cfg.use_max_time ? 1 : 0;
    cfg.auto_complete = cfg.auto_complete ? 1 : 0;
    cfg.wrap_address = cfg.wrap_address ? 1 : 0;
    if (cfg.storage_file.empty()) {
        cerr << "[警告] STORAGE_FILE 为空，恢复默认值 storage_m25p.bin" << endl;
        cfg.storage_file = "storage_m25p.bin";
    }

    // 保证派生字段和基础几何字段一致，避免配置文件里的 TOTAL_PAGES/MEMORY_SIZE
    // 与 sector/page 参数不一致时导致越界或容量错误。
    cfg.total_pages = cfg.sector_count * cfg.page_per_sector;
    cfg.memory_size = cfg.total_pages * cfg.page_size;

    cout << "\n========== Flash 芯片配置信息 ==========" << endl;
    cout << "[信息] 成功加载配置文件：" << filename << endl;
    cout << "========= M25P40 Config ==========" << endl;
    cout << "page_size: " << cfg.page_size << endl;
    cout << "page_per_sector: " << cfg.page_per_sector << endl;
    cout << "sector_count: " << cfg.sector_count << endl;
    cout << "total_pages: " << cfg.total_pages << endl;
    cout << "memory_size: " << cfg.memory_size << endl;
    cout << "f_c_mhz: " << cfg.f_c_mhz << endl;
    cout << "f_r_mhz: " << cfg.f_r_mhz << endl;
    cout << "t_prog_us: " << cfg.t_prog_us << endl;
    cout << "t_erase_sector_us: " << cfg.t_erase_sector_us << endl;
    cout << "t_erase_bulk_us: " << cfg.t_erase_bulk_us << endl;
    cout << "auto_complete: " << cfg.auto_complete << endl;
    cout << "storage_file: " << cfg.storage_file << endl;
    cout << "==================================" << endl;

    file.close();
    return cfg;
}
