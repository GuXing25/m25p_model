#include "ftl.h"
#include <iostream>

using namespace std;

FTL::FTL(FlashCore& fc, const FlashConfig& c) : core(fc), cfg(c) {}

int FTL::map_lba(int lba) {
    // 简单按页映射：LBA n -> 第 n 页起始字节地址。
    // lba 超过总页数时取模回卷，和底层 wrap_address 的思想保持一致。
    if (map.find(lba) == map.end()) {
        int total = cfg.total_pages;
        if (total <= 0) {
            cerr << "[错误] LBA 映射失败：total_pages 非法" << endl;
            return 0;
        }
        int page = lba % total;
        if (page < 0) page += total;
        map[lba] = page * cfg.page_size;
    }
    return map[lba];
}

void FTL::submit(EventType t, int addr, uint8_t* buf, int len) {
    // 使用 core.get_time() 记录入队时的当前仿真时间。
    // 事件真正执行时仍由 FlashCore 按队列顺序推进全局时间。
    q.emplace(t, addr, core.get_time(), cfg, buf, len);
}

void FTL::submit_lba(EventType t, int lba, uint8_t* buf, int len) {
    // LBA 接口主要给上层测试或未来 FTL 扩展使用，底层命令仍是字节地址。
    q.emplace(t, map_lba(lba), core.get_time(), cfg, buf, len);
}

void FTL::wait(double us) {
    // WAIT 不对应真实 SPI 命令，只用于在事件驱动模式下推进时间，
    // 例如先读到 WIP=1，再等待内部周期完成后读到 WIP=0。
    q.emplace(WAIT, 0, core.get_time(), cfg, nullptr, 0, us);
}

void FTL::run() {
    cout << "=== Run M25P40 Simulation ===" << endl;
    while (!q.empty()) {
        // FlashEvent 禁止拷贝但支持移动；这里 move 出队列头，保持写缓冲区所有权正确。
        auto e = std::move(q.front());
        q.pop();
        core.execute(e);
    }
    cout << "Done." << endl;
    cout << "Time: " << core.get_time() << " us" << endl;
    cout << "Status: 0x" << hex << uppercase << (int)core.get_status() << dec << nouppercase << endl;
}
