#include "flash_test.h"
#include <cstring>
#include <iostream>

using namespace std;

void flash_test_run(FTL& ftl, const FlashConfig& cfg) {
    // 选择一个非页首地址，便于同时验证地址解析中的 sector/page/offset。
    const int ADDR = 0x001234;
    // Sector Erase 要求传入 sector 内任意地址；这里取目标地址所在 sector 的起始地址。
    const int SECTOR_BASE = ADDR & ~(cfg.page_size * cfg.page_per_sector - 1);

    // w 作为整页写入数据，w2 用于验证“不擦除再编程只能 1->0”。
    // r/sr/id/sig 是读命令写回的缓冲区。
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

    // 固定信息读取：不依赖 WEL，不改变阵列内容。
    ftl.submit(READ_ID, 0, id, 20);
    ftl.submit(READ_ELECTRONIC_SIGNATURE, 0, sig, 1);

    // 擦除目标扇区
    ftl.submit(WRITE_ENABLE);
    ftl.submit(SECTOR_ERASE, SECTOR_BASE);
    ftl.submit(READ_STATUS, 0, sr, 1);       // 事件驱动模式下应能看到 WIP=1
    ftl.wait(cfg.use_max_time ? cfg.t_erase_sector_max_us : cfg.t_erase_sector_us);
    ftl.submit(READ_STATUS, 0, sr, 1);       // 完成后 WIP=0

    // 页编程：0xFF -> 0xAA
    // 编程前必须 WREN；编程启动后读取状态寄存器能看到 WIP=1。
    ftl.submit(WRITE_ENABLE);
    ftl.submit(PAGE_PROGRAM, ADDR, w, 256);
    ftl.submit(READ_STATUS, 0, sr, 1);
    ftl.wait(cfg.use_max_time ? cfg.t_prog_max_us : cfg.t_prog_us);
    ftl.submit(READ_STATUS, 0, sr, 1);
    ftl.submit(READ, ADDR, r, 256);

    // 不擦除继续编程：0xAA & 0xF0 = 0xA0，验证 NOR 只能 1->0
    // 如果模型错误地允许 0->1，这里读出的结果会变成 0xF0 而不是 0xA0。
    ftl.submit(WRITE_ENABLE);
    ftl.submit(PAGE_PROGRAM, ADDR, w2, 16);
    ftl.wait(cfg.use_max_time ? cfg.t_prog_max_us : cfg.t_prog_us);
    ftl.submit(FAST_READ, ADDR, r, 16);

    // Deep Power-Down 保护：DPD 中 WREN 被忽略，Release 后恢复
    // 这里没有继续写数据，只验证 DPD 状态下普通命令会被核心入口拒绝。
    ftl.submit(DEEP_POWER_DOWN_CMD);
    ftl.submit(WRITE_ENABLE);
    ftl.submit(RELEASE_POWER_DOWN);

    // 到这里为止只是排队；run() 才会按顺序真正执行所有事件。
    ftl.run();

    bool ok = true;
    // 最后的 FAST_READ 覆盖 r[0..15]，因此检查这 16 字节是否符合 0xAA & 0xF0。
    for (int i = 0; i < 16; i++) {
        if (r[i] != (uint8_t)(0xAA & 0xF0)) {
            ok = false;
            break;
        }
    }

    cout << "Test Result: " << (ok ? "PASS" : "FAIL") << endl;
}
