#include "ftl.h"
#include <iostream>

using namespace std;

FTL::FTL(FlashCore& fc, const FlashConfig& c) : core(fc), cfg(c) {}

int FTL::map_lba(int lba) {
    if (map.find(lba) == map.end()) {
        int total = cfg.total_pages;
        map[lba] = (lba % total) * cfg.page_size;
    }
    return map[lba];
}

void FTL::submit(EventType t, int addr, uint8_t* buf, int len) {
    q.emplace(t, addr, core.get_time(), cfg, buf, len);
}

void FTL::submit_lba(EventType t, int lba, uint8_t* buf, int len) {
    q.emplace(t, map_lba(lba), core.get_time(), cfg, buf, len);
}

void FTL::wait(double us) {
    q.emplace(WAIT, 0, core.get_time(), cfg, nullptr, 0, us);
}

void FTL::run() {
    cout << "=== Run M25P40 Simulation ===" << endl;
    while (!q.empty()) {
        auto e = std::move(q.front());
        q.pop();
        core.execute(e);
    }
    cout << "Done." << endl;
    cout << "Time: " << core.get_time() << " us" << endl;
    cout << "Status: 0x" << hex << uppercase << (int)core.get_status() << dec << nouppercase << endl;
}
