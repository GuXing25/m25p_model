#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include <string>

// Flash 芯片配置结构体：M25P40 NOR Flash
struct FlashConfig {
    int page_size;
    int page_per_sector;
    int sector_count;
    int total_pages;
    int memory_size;

    double f_c_mhz;        // 除普通 READ 外命令最大频率
    double f_r_mhz;        // 普通 READ 最大频率
    double t_shsl_us;      // S# deselect time

    double t_read_us;      // 兼容原框架字段；实际 READ 按总线位数计算
    double t_fast_read_us;
    double t_w_us;         // WRITE STATUS REGISTER 内部周期
    double t_w_max_us;
    double t_prog_us;      // PAGE PROGRAM 256B 典型内部周期
    double t_prog_max_us;
    double t_prog_chunk_us;// tPP(n) 估算：ceil(n/8) * 25us
    double t_erase_sector_us;
    double t_erase_sector_max_us;
    double t_erase_bulk_us;
    double t_erase_bulk_max_us;
    double t_dpd_us;
    double t_res_us;
    double t_vsl_us;
    double t_puw_us;

    int use_max_time;      // 0=typ，1=max
    int auto_complete;     // 0=事件驱动 busy/WIP，1=兼容原仓库阻塞式完成
    int wrap_address;      // 1=地址按容量回卷

    std::string storage_file;

    FlashConfig();
};

// 解析 .conf 配置文件
FlashConfig load_config(const std::string& filename);

#endif
