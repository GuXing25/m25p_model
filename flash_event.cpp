#include "flash_event.h"
#include <cstring>

FlashEvent::FlashEvent(EventType t, int a, double s, const FlashConfig& c,
                       uint8_t* b, int l, double wait)
    : type(t), addr(a), start(s), dur(0.0), end(s), wait_us(wait),
      buf(nullptr), len(l), owns_buf(false) {
    // 队列式事件必须复制写入类 buffer，避免上层复用缓冲区导致数据被篡改
    if ((type == PAGE_PROGRAM || type == WRITE_STATUS) && len > 0 && b) {
        buf = new uint8_t[len];
        memcpy(buf, b, len);
        owns_buf = true;
    } else {
        buf = b;
    }

    switch (type) {
        case READ: dur = c.t_read_us; break;
        case FAST_READ: dur = c.t_fast_read_us; break;
        case PAGE_PROGRAM: dur = c.t_prog_us; break;
        case SECTOR_ERASE: dur = c.t_erase_sector_us; break;
        case BULK_ERASE: dur = c.t_erase_bulk_us; break;
        case WRITE_STATUS: dur = c.t_w_us; break;
        case WAIT: dur = wait_us; break;
        default: dur = 0.0; break;
    }
    end = start + dur;
}

FlashEvent::FlashEvent(FlashEvent&& other) noexcept {
    type = other.type;
    addr = other.addr;
    start = other.start;
    dur = other.dur;
    end = other.end;
    wait_us = other.wait_us;
    buf = other.buf;
    len = other.len;
    owns_buf = other.owns_buf;

    other.buf = nullptr;
    other.len = 0;
    other.owns_buf = false;
}

FlashEvent::~FlashEvent() {
    if (owns_buf) delete[] buf;
}
