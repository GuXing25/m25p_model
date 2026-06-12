# M25P40 NOR Flash Model

该目录按照给定 GitHub 仓库的 C++ 分层风格生成：`config_parser`、`flash_hardware`、`flash_event`、`flash_core`、`ftl`、`flash_test`、`main`。

M25P40 属于 SPI NOR Flash，因此模型采用 NOR 结构：

```text
FlashChip -> Sector -> Page
8 sectors, 256 pages/sector, 256 bytes/page, total 512KiB
```

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

构建与运行：

```bash
make
make run
```

默认配置文件为 `m25p.conf`，默认阵列文件为 `storage_m25p.bin`。

该版本计入了FLASH接口、少数封装带来的约束。计入了状态寄存器、WEL