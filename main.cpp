#include <fstream>
#include <iostream>
#include "config_parser.h"
#include "flash_core.h"
#include "ftl.h"
#include "flash_test.h"

using namespace std;

int main() {
    // 1. 加载配置：如果 m25p.conf 不存在，会使用 FlashConfig 的 M25P40 默认参数。
    FlashConfig cfg = load_config("m25p.conf");

    // 2. 创建芯片内存结构和命令核心。FlashCore 构造时会挂载 storage_file。
    FlashChip chip(cfg);
    FlashCore core(chip, cfg);

    // 3. FTL 在这里作为命令队列/地址映射层，测试用例通过它提交事件。
    FTL ftl(core, cfg);

    // 4. 运行示例测试序列，覆盖主要 NOR Flash 行为。
    flash_test_run(ftl, cfg);

    // 5. 简单确认后端阵列文件存在，说明模型已经创建或打开了持久化镜像。
    ifstream f(cfg.storage_file, ios::binary);
    if (f) cout << "[OK] " << cfg.storage_file << " created!" << endl;
    return 0;
}
