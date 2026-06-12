#ifndef FLASH_EVENT_H
#define FLASH_EVENT_H

#include "config_parser.h"
#include <cstdint>

// M25P40 命令级事件。
//
// 仿真器把一次 SPI 命令抽象成一个 FlashEvent，由 FTL 排队后交给
// FlashCore 执行。WRITE/ERASE 是对 PAGE_PROGRAM/SECTOR_ERASE 的兼容别名，
// 便于复用原仓库可能已有的调用方式。
enum EventType {
    WRITE_ENABLE,
    WRITE_DISABLE,
    READ_STATUS,
    WRITE_STATUS,
    READ,
    FAST_READ,
    PAGE_PROGRAM,
    WRITE = PAGE_PROGRAM,
    SECTOR_ERASE,
    ERASE = SECTOR_ERASE,
    BULK_ERASE,
    DEEP_POWER_DOWN_CMD,
    RELEASE_POWER_DOWN,
    READ_ID,
    READ_ELECTRONIC_SIGNATURE,
    WAIT
};

class FlashEvent {
private:
    // 命令类型和命令地址。对于 READ/PP/SE，addr 是字节地址；
    // 对 WREN/RDSR/BULK ERASE 等无地址命令，addr 通常为 0。
    EventType type;
    int addr;

    // start/dur/end 保留事件时间窗信息。FlashCore 当前以自己的
    // current_operation_time 为准推进时钟，这里主要用于兼容和调试。
    double start;
    double dur;
    double end;

    // WAIT 事件使用 wait_us 主动推进仿真时间，用来观察 WIP 轮询过程。
    double wait_us;

    // 命令数据缓冲区。
    // 读命令直接写入调用者传入的 buf；写命令会复制 buf，避免事件排队后
    // 上层修改原数组导致待执行数据变化。
    uint8_t* buf;
    int len;
    bool owns_buf;

public:
    FlashEvent(EventType t, int a, double s, const FlashConfig& c,
               uint8_t* b = nullptr, int l = 0, double wait = 0.0);
    FlashEvent(FlashEvent&& other) noexcept;
    FlashEvent(const FlashEvent&) = delete;
    FlashEvent& operator=(const FlashEvent&) = delete;
    ~FlashEvent();

    EventType getType() const { return type; }
    int getAddr() const { return addr; }
    uint8_t* getBuf() const { return buf; }
    int getLen() const { return len; }
    double getEnd() const { return end; }
    double getWaitUs() const { return wait_us; }
};

#endif
