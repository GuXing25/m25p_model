#ifndef FTL_H
#define FTL_H

#include "flash_core.h"
#include "flash_event.h"
#include <queue>
#include <unordered_map>

class FTL {
private:
    // 本项目保留 FTL 作为上层 controller：它不直接改 Flash，
    // 而是负责命令排队、简单 LBA 映射和仿真时间调度。
    FlashCore& core;
    const FlashConfig& cfg;
    std::queue<FlashEvent> q;

    // 简单 LBA->字节地址映射表。当前还不是完整 SSD FTL，
    // 但保留该层便于后续扩展 controller 策略、磨损统计或映射策略。
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
