#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include <string>

// Flash 芯片配置结构体：M25P40 NOR Flash
//
// 这个结构体把芯片几何参数、SPI 时钟、内部自定时周期和仿真行为开关
// 集中到一处。load_config() 会先用构造函数给出的 M25P40 默认值初始化，
// 再用 m25p.conf 覆盖其中出现的字段。
struct FlashConfig {
    // 几何参数：M25P40 = 4 Mbit = 512 KiB。
    // page_size、page_per_sector、sector_count 是基础输入；
    // total_pages 和 memory_size 会在读取配置后按前三者重新计算。
    int page_size;
    int page_per_sector;
    int sector_count;
    int total_pages;
    int memory_size;

    // SPI 总线时序参数，单位见字段名：
    // f_c_mhz 用于大多数命令；f_r_mhz 用于普通 READ(03h)。
    double f_c_mhz;        // 除普通 READ 外命令最大频率
    double f_r_mhz;        // 普通 READ 最大频率
    double t_shsl_us;      // S# deselect time

    // 内部自定时周期，单位 us。
    // 读命令主要由总线位数和频率决定，t_read_us/t_fast_read_us
    // 仅用于兼容旧接口的事件 dur 字段。
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

    // 仿真行为开关：
    // use_max_time=1 时采用 datasheet 中的最大时间；
    // auto_complete=1 时写/擦除启动后立即推进到完成时间；
    // wrap_address=1 时地址超过容量后按芯片容量回卷。
    int use_max_time;      // 0=typ，1=max
    int auto_complete;     // 0=事件驱动 busy/WIP，1=兼容原仓库阻塞式完成
    int wrap_address;      // 1=地址按容量回卷

    // 后端存储文件。FlashCore 会把它当成 512 KiB NOR 阵列镜像。
    std::string storage_file;

    FlashConfig();
};

// 解析 .conf 配置文件
FlashConfig load_config(const std::string& filename);

#endif
