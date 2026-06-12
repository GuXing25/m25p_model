# M25P40 NOR Flash Model

该目录按照给定 GitHub 仓库的 C++ 分层风格生成：`config_parser`、`flash_hardware`、`flash_event`、`flash_core`、`ftl`、`flash_test`、`main`。

M25P40 属于 SPI NOR Flash，因此模型采用 NOR 结构：

```text
FTL / Controller -> FlashCore -> FlashChip -> Sector -> Page
8 sectors, 256 pages/sector, 256 bytes/page, total 512KiB
```

本项目保留 `FTL` 层作为上层 controller 抽象。它负责命令排队、简单 LBA 到字节地址映射、`WAIT` 调度和驱动 `FlashCore` 顺序执行；完整磨损均衡、垃圾回收和复杂映射策略可以在这一层继续扩展。

已实现的主要行为：

- `WRITE_ENABLE` / `WRITE_DISABLE`
- `READ_STATUS` / `WRITE_STATUS`
- `READ` / `FAST_READ`
- `PAGE_PROGRAM`，按 `mem &= data` 建模，只允许 bit 从 1 到 0
- 页内回卷，超过 256B 时后续数据覆盖同一页缓冲区
- `SECTOR_ERASE`，64KiB 扇区擦除到 `0xFF`
- `BULK_ERASE`，全片擦除到 `0xFF`
- BP2/BP1/BP0 软件保护区约束
- SRWD + W# 硬件保护状态寄存器约束
- WIP/WEL 状态寄存器行为
- Deep Power-Down / Release / Read Electronic Signature
- JEDEC ID：`20 20 13`，RES：`12`
- 事件驱动 busy 模式，支持 `WAIT` 和 busy 中 `READ_STATUS` 轮询
- 启动时从 `storage_m25p.bin` 同步阵列内容到内存镜像
- 配置合法性检查，避免非法页大小、容量、频率或时间参数破坏模型
- `WRAP_ADDRESS=0` 时拒绝越界读写擦命令

构建与运行：

```bash
make
make run
```

默认配置文件为 `m25p.conf`，默认阵列文件为 `storage_m25p.bin`。

该版本计入了 Flash 接口、少数封装带来的约束，也计入了状态寄存器、WEL/WIP、保护位和 Deep Power-Down 等芯片级行为。
未验证！！！！！！！