#ifndef FLASH_CORE_H
#define FLASH_CORE_H

#include "flash_hardware.h"
#include "flash_event.h"
#include <fstream>
#include <vector>

struct PendingOperation {
    // active=true 表示芯片正处于 WIP=1 的内部自定时周期。
    bool active;
    EventType type;

    // addr/len 记录启动内部周期时的目标地址和请求长度。
    // 对擦除类操作，len 可以为 0；对写状态寄存器，sr_value 保存待写值。
    int addr;
    int len;
    double complete_time;
    uint8_t sr_value;

    // PAGE_PROGRAM 使用 data 保存页缓冲区，valid 标记哪些 offset 被本次命令写入。
    // 这样可以准确模拟“页内回卷”和“未写入位置保持原值”。
    std::vector<uint8_t> data;
    std::vector<uint8_t> valid;

    PendingOperation();
};

// Flash 核心：M25P40 NOR 行为模型，保留原仓库 FlashCore + storage 文件风格。
//
// FlashCore 负责四类事情：
// 1. SPI 命令协议：WREN/RDSR/READ/PP/SE/BE/DPD 等；
// 2. 状态寄存器：WIP、WEL、BP、SRWD 以及 W# 硬件保护；
// 3. 时间推进：总线传输时间 + 写/擦除内部自定时周期；
// 4. 数据一致性：把 storage_file 后端和 FlashChip 内存镜像同步。
class FlashCore {
private:
    FlashChip& chip;
    FlashConfig cfg;
    std::fstream storage_file;

    // 全局仿真时间，单位 us。每个命令根据总线传输位数和内部周期推进它。
    double current_operation_time;

    // 当前挂起的写/擦除/写状态寄存器操作。READ_STATUS 可以在 pending 期间轮询 WIP。
    PendingOperation pending;

    // M25P40 状态寄存器位定义。
    static const uint8_t SR_WIP  = 0x01;
    static const uint8_t SR_WEL  = 0x02;
    static const uint8_t SR_BP0  = 0x04;
    static const uint8_t SR_BP1  = 0x08;
    static const uint8_t SR_BP2  = 0x10;
    static const uint8_t SR_SRWD = 0x80;

    // 地址、后端文件和保护状态辅助函数。
    void init_storage();
    void parse_address(int addr, int& sector, int& page, int& offset);
    int normalize_addr(int addr) const;
    int sector_size() const;

    // 状态寄存器读写和保护判断。
    bool is_wip() const;
    bool is_wel() const;
    int bp_value() const;
    void set_wip(bool v);
    void set_wel(bool v);
    bool hardware_protected() const;
    bool sector_protected(int sector) const;

    // 时序计算。总线时间按 SPI clock 数换算，内部周期按配置取 typ/max。
    double clocks_to_us(int clocks, double freq_mhz) const;
    double page_program_time_us(int n) const;
    double command_bus_time(const FlashEvent& event) const;
    void add_time(double us);

    // 只读命令的数据填充。
    void read_bytes(int addr, uint8_t* buf, int len);
    void read_status(uint8_t* buf, int len);
    void read_id(uint8_t* buf, int len);
    void read_signature(uint8_t* buf, int len);

    // 写/擦除类命令的两阶段执行：先 start_pending() 进入 WIP，
    // 时间到后由 complete_pending_if_ready() 调用 apply_* 真正修改阵列。
    bool start_pending(EventType type, int addr, int len,
                       const std::vector<uint8_t>& data,
                       const std::vector<uint8_t>& valid,
                       uint8_t sr_value,
                       double bus_time_us,
                       double internal_time_us);
    void complete_pending_if_ready();
    void apply_page_program();
    void apply_sector_erase();
    void apply_bulk_erase();
    void apply_write_status();
    void update_page_status(int sector, int page);

    // 各条 SPI 命令的具体执行函数。execute() 会根据 EventType 分发到这些函数。
    void execute_write_enable();
    void execute_write_disable();
    void execute_read_status(FlashEvent& event);
    void execute_write_status(FlashEvent& event);
    void execute_read(FlashEvent& event, bool fast);
    void execute_page_program(FlashEvent& event);
    void execute_sector_erase(FlashEvent& event);
    void execute_bulk_erase(FlashEvent& event);
    void execute_deep_power_down();
    void execute_release_power_down();
    void execute_read_id(FlashEvent& event);
    void execute_read_signature(FlashEvent& event);
    void execute_wait(FlashEvent& event);

public:
    FlashCore(FlashChip& c, const FlashConfig& conf);
    ~FlashCore();

    void execute(FlashEvent& event);
    double get_time() const;
    bool is_busy() const;
    uint8_t get_status() const;
    void set_write_protect(bool low);
};

#endif
