#ifndef FTL_H
#define FTL_H

#include "flash_core.h"
#include "flash_event.h"
#include <queue>
#include <unordered_map>

class FTL {
private:
    // FTL 是一个很薄的事件调度层：它不直接改 Flash，只负责把命令排队。
    FlashCore& core;
    const FlashConfig& cfg;
    std::queue<FlashEvent> q;

    // 简单 LBA->字节地址映射表。这里没有实现真实磨损均衡，只是为了
    // 给上层提供按页 LBA 提交命令的兼容入口。
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
