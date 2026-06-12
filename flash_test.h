#ifndef FLASH_TEST_H
#define FLASH_TEST_H

#include "ftl.h"

// 运行一组端到端命令序列，覆盖 M25P40 模型的关键行为：
// ID/签名读取、扇区擦除、页编程、WIP 轮询、NOR 1->0 约束和 DPD 保护。
void flash_test_run(FTL& ftl, const FlashConfig& cfg);

#endif
