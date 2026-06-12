#include "flash_event.h"
#include <cstring>

FlashEvent::FlashEvent(EventType t, int a, double s, const FlashConfig& c,
                       uint8_t* b, int l, double wait)
    : type(t), addr(a), start(s), dur(0.0), end(s), wait_us(wait),
      buf(nullptr), len(l), owns_buf(false) {
    // 队列式事件必须复制写入类 buffer，避免上层复用缓冲区导致数据被篡改
    // READ/FAST_READ/RDSR/ID 等读命令不复制 buffer，因为它们需要把结果写回调用者。
    if ((type == PAGE_PROGRAM || type == WRITE_STATUS) && len > 0 && b) {
        buf = new uint8_t[len];
        memcpy(buf, b, len);
        owns_buf = true;
    } else {
        buf = b;
    }

    // dur/end 是事件自身的名义耗时。FlashCore 会重新按命令总线位数
    // 和配置的内部周期计算真实推进时间，因此这里更多是兼容旧接口。
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

// FlashEvent 需要支持放入 std::queue 后再 move 出来执行。
// move 构造会转移 buf 所有权，避免写命令复制出的缓冲区被重复释放。
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

// 只有 owns_buf=true 的写命令事件才释放 buf；读命令的 buf 由调用者持有。
FlashEvent::~FlashEvent() {
    if (owns_buf) delete[] buf;
}
