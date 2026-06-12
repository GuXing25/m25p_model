#ifndef FLASH_EVENT_H
#define FLASH_EVENT_H

#include "config_parser.h"
#include <cstdint>

// M25P40 命令级事件；WRITE/ERASE 作为原仓库接口的兼容别名保留
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
    EventType type;
    int addr;
    double start;
    double dur;
    double end;
    double wait_us;
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
