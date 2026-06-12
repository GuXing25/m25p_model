#ifndef FLASH_CORE_H
#define FLASH_CORE_H

#include "flash_hardware.h"
#include "flash_event.h"
#include <fstream>
#include <vector>

struct PendingOperation {
    bool active;
    EventType type;
    int addr;
    int len;
    double complete_time;
    uint8_t sr_value;
    std::vector<uint8_t> data;
    std::vector<uint8_t> valid;

    PendingOperation();
};

// Flash 核心：M25P40 NOR 行为模型，保留原仓库 FlashCore + storage 文件风格
class FlashCore {
private:
    FlashChip& chip;
    FlashConfig cfg;
    std::fstream storage_file;
    double current_operation_time;
    PendingOperation pending;

    static const uint8_t SR_WIP  = 0x01;
    static const uint8_t SR_WEL  = 0x02;
    static const uint8_t SR_BP0  = 0x04;
    static const uint8_t SR_BP1  = 0x08;
    static const uint8_t SR_BP2  = 0x10;
    static const uint8_t SR_SRWD = 0x80;

    void init_storage();
    void parse_address(int addr, int& sector, int& page, int& offset);
    int normalize_addr(int addr) const;
    int sector_size() const;

    bool is_wip() const;
    bool is_wel() const;
    int bp_value() const;
    void set_wip(bool v);
    void set_wel(bool v);
    bool hardware_protected() const;
    bool sector_protected(int sector) const;

    double clocks_to_us(int clocks, double freq_mhz) const;
    double page_program_time_us(int n) const;
    double command_bus_time(const FlashEvent& event) const;
    void add_time(double us);

    void read_bytes(int addr, uint8_t* buf, int len);
    void read_status(uint8_t* buf, int len);
    void read_id(uint8_t* buf, int len);
    void read_signature(uint8_t* buf, int len);

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
