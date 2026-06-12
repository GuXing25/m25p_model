#ifndef FLASH_HARDWARE_H
#define FLASH_HARDWARE_H

#include "config_parser.h"
#include <cstdint>

// Page 状态（NOR：擦除态 / 已编程 / 无效）
// 当前模型主要使用 FREE/VALID；INVALID 作为保留状态，便于后续扩展 FTL。
enum PageStatus {
    FREE,
    VALID,
    INVALID
};

// M25P40 页：256B，单页可编程，不可页擦除。
// data 是内存中的镜像，真正持久化的数据由 FlashCore 同步到 storage_file。
struct Page {
    int id;
    PageStatus status;
    uint8_t* data;
    Page(int pid, int page_size);
    ~Page();
};

// M25P40 扇区：64KB = 256 x 256B Page，是最小擦除单元。
// erase() 会把扇区内所有 page 恢复为 NOR 擦除态 0xFF。
struct Sector {
    int id;
    Page** pages;
    const FlashConfig& cfg;
    Sector(int sid, const FlashConfig& config);
    ~Sector();
    void erase();
};

// 芯片状态：保留原仓库 IDLE/BUSY 风格，扩展 DPD。
// BUSY 对应 WIP=1 的内部自定时周期
// DEEP_POWER_DOWN 对应低功耗保护状态
enum ChipState {
    IDLE,
    BUSY,
    DEEP_POWER_DOWN
};

// Flash 芯片结构：M25P40 是 NOR，结构为 Chip -> Sector -> Page。
// 这个结构只保存“硬件可见状态”和内存镜像；命令协议、时序推进、
// 保护位判断等行为由 FlashCore 实现。
struct FlashChip {
    Sector** sectors;
    ChipState state;
    double time;
    bool busy;
    bool deep_power_down;
    bool write_protect_low; // W# = LOW 时为 true
    uint8_t status_reg;     // b7 SRWD, b4..b2 BP2..BP0, b1 WEL, b0 WIP
    const FlashConfig& cfg;

    FlashChip(const FlashConfig& config);
    ~FlashChip();
};

#endif
