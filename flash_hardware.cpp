#include "flash_hardware.h"
#include <cstring>

using namespace std;

// ==================== Page ====================
Page::Page(int pid, int page_size) : id(pid), status(FREE) {
    data = new uint8_t[page_size];
    memset(data, 0xFF, page_size);
}

Page::~Page() {
    delete[] data;
}

// ==================== Sector (M25P40 最小擦除单元) ====================
Sector::Sector(int sid, const FlashConfig& config) : id(sid), cfg(config) {
    pages = new Page*[cfg.page_per_sector];
    for (int i = 0; i < cfg.page_per_sector; i++) {
        pages[i] = new Page(i, cfg.page_size);
    }
}

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
FlashChip::FlashChip(const FlashConfig& config)
    : state(IDLE), time(0.0), busy(false), deep_power_down(false),
      write_protect_low(false), status_reg(0x00), cfg(config) {
    sectors = new Sector*[cfg.sector_count];
    for (int i = 0; i < cfg.sector_count; i++) {
        sectors[i] = new Sector(i, cfg);
    }
}

FlashChip::~FlashChip() {
    for (int i = 0; i < cfg.sector_count; i++) {
        delete sectors[i];
    }
    delete[] sectors;
}
