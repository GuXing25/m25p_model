#include "flash_test.h"
#include <cstring>
#include <iostream>

using namespace std;

void flash_test_run(FTL& ftl, const FlashConfig& cfg) {
    const int ADDR = 0x001234;
    const int SECTOR_BASE = ADDR & ~(cfg.page_size * cfg.page_per_sector - 1);

    uint8_t w[256];
    uint8_t w2[16];
    uint8_t r[256];
    uint8_t sr[2];
    uint8_t id[20];
    uint8_t sig[1];

    memset(w, 0xAA, sizeof(w));
    memset(w2, 0xF0, sizeof(w2));
    memset(r, 0x00, sizeof(r));
    memset(sr, 0x00, sizeof(sr));
    memset(id, 0x00, sizeof(id));
    memset(sig, 0x00, sizeof(sig));

    cout << "[测试] M25P40 NOR 行为：WREN -> SE -> PP -> RDSR -> READ -> 1->0 再编程" << endl;

    ftl.submit(READ_ID, 0, id, 20);
    ftl.submit(READ_ELECTRONIC_SIGNATURE, 0, sig, 1);

    // 擦除目标扇区
    ftl.submit(WRITE_ENABLE);
    ftl.submit(SECTOR_ERASE, SECTOR_BASE);
    ftl.submit(READ_STATUS, 0, sr, 1);       // 事件驱动模式下应能看到 WIP=1
    ftl.wait(cfg.use_max_time ? cfg.t_erase_sector_max_us : cfg.t_erase_sector_us);
    ftl.submit(READ_STATUS, 0, sr, 1);       // 完成后 WIP=0

    // 页编程：0xFF -> 0xAA
    ftl.submit(WRITE_ENABLE);
    ftl.submit(PAGE_PROGRAM, ADDR, w, 256);
    ftl.submit(READ_STATUS, 0, sr, 1);
    ftl.wait(cfg.use_max_time ? cfg.t_prog_max_us : cfg.t_prog_us);
    ftl.submit(READ_STATUS, 0, sr, 1);
    ftl.submit(READ, ADDR, r, 256);

    // 不擦除继续编程：0xAA & 0xF0 = 0xA0，验证 NOR 只能 1->0
    ftl.submit(WRITE_ENABLE);
    ftl.submit(PAGE_PROGRAM, ADDR, w2, 16);
    ftl.wait(cfg.use_max_time ? cfg.t_prog_max_us : cfg.t_prog_us);
    ftl.submit(FAST_READ, ADDR, r, 16);

    // Deep Power-Down 保护：DPD 中 WREN 被忽略，Release 后恢复
    ftl.submit(DEEP_POWER_DOWN_CMD);
    ftl.submit(WRITE_ENABLE);
    ftl.submit(RELEASE_POWER_DOWN);

    ftl.run();

    bool ok = true;
    for (int i = 0; i < 16; i++) {
        if (r[i] != (uint8_t)(0xAA & 0xF0)) {
            ok = false;
            break;
        }
    }

    cout << "Test Result: " << (ok ? "PASS" : "FAIL") << endl;
}
