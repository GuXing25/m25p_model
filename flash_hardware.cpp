#include "flash_hardware.h"
#include <cstring>

using namespace std;

// ==================== Page ====================
// 构造一个页的内存镜像。NOR Flash 擦除后的物理状态是所有 bit 为 1，
// 因此初始数据填充为 0xFF，状态标记为 FREE。
Page::Page(int pid, int page_size) : id(pid), status(FREE) {
    data = new uint8_t[page_size];
    memset(data, 0xFF, page_size);
}

// Page 拥有自己的 data 缓冲区，析构时释放。
Page::~Page() {
    delete[] data;
}

// ==================== Sector (M25P40 最小擦除单元) ====================
// 按配置创建一个 sector 内的所有 page。M25P40 中 sector 是擦除粒度，
// page 是编程粒度，所以这里用二维层级保存内存镜像。
Sector::Sector(int sid, const FlashConfig& config) : id(sid), cfg(config) {
    pages = new Page*[cfg.page_per_sector];
    for (int i = 0; i < cfg.page_per_sector; i++) {
        pages[i] = new Page(i, cfg.page_size);
    }
}

// Sector 拥有 pages 数组以及其中每一个 Page 对象。
Sector::~Sector() {
    for (int i = 0; i < cfg.page_per_sector; i++) {
        delete pages[i];
    }
    delete[] pages;
}

// NOR 擦除：只能按 Sector 擦除，所有 bit 变成 1，即 0xFF
void Sector::erase() {
    for (int i = 0; i < cfg.page_per_sector; i++) {
        memset(pages[i]->data, 0xFF, cfg.page_size);
        pages[i]->status = FREE;
    }
}

// ==================== FlashChip ====================
// FlashChip 保存芯片级状态寄存器、W# 引脚状态、DPD 状态以及所有 sector。
// 真正的命令执行不放在这里，是为了让数据结构和协议行为保持分离。
FlashChip::FlashChip(const FlashConfig& config)
    : state(IDLE), time(0.0), busy(false), deep_power_down(false),
      write_protect_low(false), status_reg(0x00), cfg(config) {
    sectors = new Sector*[cfg.sector_count];
    for (int i = 0; i < cfg.sector_count; i++) {
        sectors[i] = new Sector(i, cfg);
    }
}

// 释放整个 Chip -> Sector -> Page 层级。
FlashChip::~FlashChip() {
    for (int i = 0; i < cfg.sector_count; i++) {
        delete sectors[i];
    }
    delete[] sectors;
}
