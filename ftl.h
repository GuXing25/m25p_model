#ifndef FTL_H
#define FTL_H

#include "flash_core.h"
#include "flash_event.h"
#include <queue>
#include <unordered_map>

class FTL {
private:
    FlashCore& core;
    const FlashConfig& cfg;
    std::queue<FlashEvent> q;
    std::unordered_map<int, int> map;

public:
    FTL(FlashCore& fc, const FlashConfig& c);
    int map_lba(int lba);

    // M25P40 是 SPI NOR，addr 按字节地址解释；READ/FAST_READ/PP/SE 等直接用该地址
    void submit(EventType t, int addr = 0, uint8_t* buf = nullptr, int len = 0);
    void submit_lba(EventType t, int lba, uint8_t* buf = nullptr, int len = 0);
    void wait(double us);
    void run();
};

#endif
