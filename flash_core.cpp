#include "flash_core.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <vector>

using namespace std;

static void print_data_hex(const char* title, const uint8_t* data, int len) {
    cout << title << " (" << len << " bytes):" << endl;
    if (data == nullptr || len <= 0) {
        cout << "[空缓冲区]" << endl;
        return;
    }
    int shown = min(len, 32);
    for (int i = 0; i < shown; i++) {
        printf("%02X ", data[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    if (shown % 16 != 0) printf("\n");
    if (len > shown) cout << "..." << endl;
}

PendingOperation::PendingOperation()
    : active(false), type(READ), addr(0), len(0), complete_time(0.0), sr_value(0) {}

FlashCore::FlashCore(FlashChip& c, const FlashConfig& conf)
    : chip(c), cfg(conf), current_operation_time(0.0) {
    init_storage();
}

FlashCore::~FlashCore() {
    if (storage_file.is_open()) {
        storage_file.flush();
        storage_file.close();
    }
}

// 初始化 M25P40 存储文件（全 0xFF，容量 512KiB）
void FlashCore::init_storage() {
    storage_file.open(cfg.storage_file, ios::in | ios::out | ios::binary);
    if (!storage_file.is_open()) {
        cout << "[信息] 未找到 " << cfg.storage_file << "，创建新文件..." << endl;
        storage_file.open(cfg.storage_file, ios::out | ios::binary | ios::trunc);
        if (!storage_file.is_open()) {
            cerr << "[错误] 创建 " << cfg.storage_file << " 失败！" << endl;
            return;
        }
        vector<uint8_t> init_data(cfg.page_size, 0xFF);
        for (int i = 0; i < cfg.total_pages; i++) {
            storage_file.write((char*)init_data.data(), cfg.page_size);
        }
        storage_file.close();
        storage_file.open(cfg.storage_file, ios::in | ios::out | ios::binary);
    } else {
        storage_file.seekg(0, ios::end);
        streamoff size = storage_file.tellg();
        if (size < cfg.memory_size) {
            cout << "[警告] 存储文件容量不足，重新初始化为擦除态" << endl;
            storage_file.close();
            storage_file.open(cfg.storage_file, ios::out | ios::binary | ios::trunc);
            vector<uint8_t> init_data(cfg.page_size, 0xFF);
            for (int i = 0; i < cfg.total_pages; i++) {
                storage_file.write((char*)init_data.data(), cfg.page_size);
            }
            storage_file.close();
            storage_file.open(cfg.storage_file, ios::in | ios::out | ios::binary);
        }
    }

    cout << "[信息] M25P40 存储文件已挂载" << endl;
    cout << "[时间] 初始时间：" << current_operation_time << "us" << endl;
}

int FlashCore::sector_size() const {
    return cfg.page_size * cfg.page_per_sector;
}

int FlashCore::normalize_addr(int addr) const {
    if (cfg.memory_size <= 0) return 0;
    if (!cfg.wrap_address) return addr;
    int a = addr % cfg.memory_size;
    if (a < 0) a += cfg.memory_size;
    return a;
}

// M25P40地址解析：字节地址 -> Sector / Page / Offset
void FlashCore::parse_address(int addr, int& sector, int& page, int& offset) {
    int a = normalize_addr(addr);
    int page_addr = a / cfg.page_size;
    sector = page_addr / cfg.page_per_sector;
    page = page_addr % cfg.page_per_sector;
    offset = a % cfg.page_size;
}

bool FlashCore::is_wip() const {
    return (chip.status_reg & SR_WIP) != 0;
}

bool FlashCore::is_wel() const {
    return (chip.status_reg & SR_WEL) != 0;
}

int FlashCore::bp_value() const {
    return (chip.status_reg >> 2) & 0x07;
}

void FlashCore::set_wip(bool v) {
    if (v) chip.status_reg |= SR_WIP;
    else chip.status_reg &= ~SR_WIP;
}

void FlashCore::set_wel(bool v) {
    if (v) chip.status_reg |= SR_WEL;
    else chip.status_reg &= ~SR_WEL;
}

bool FlashCore::hardware_protected() const {
    return ((chip.status_reg & SR_SRWD) != 0) && chip.write_protect_low;
}

// BP2/BP1/BP0 保护高地址扇区：001保护7，010保护6-7，011保护4-7，1xx保护全部
bool FlashCore::sector_protected(int sector) const {
    int bp = bp_value();
    if (bp == 0) return false;
    if (bp == 1) return sector >= 7;
    if (bp == 2) return sector >= 6;
    if (bp == 3) return sector >= 4;
    return true;
}

// MHz 下 clocks 个 SPI clock 的耗时，单位 us
// 1 clock / 1 MHz = 1 us
// 因此 clocks / MHz = us
// 这与前面提取的 Tbus(bits,fMHz) 公式一致

double FlashCore::clocks_to_us(int clocks, double freq_mhz) const {
    if (freq_mhz <= 0.0) return 0.0;
    return (double)clocks / freq_mhz;
}

// M25P40 页编程内部周期：256B 典型 0.8ms；nB 按 ceil(n/8)*25us 估算
// 如果 USE_MAX_TIME=1，则使用 5ms 最坏值

double FlashCore::page_program_time_us(int n) const {
    if (cfg.use_max_time) return cfg.t_prog_max_us;
    if (n >= cfg.page_size) return cfg.t_prog_us;
    int chunks = (int)ceil(max(1, n) / 8.0);
    return chunks * cfg.t_prog_chunk_us;
}

double FlashCore::command_bus_time(const FlashEvent& event) const {
    int len = max(0, event.getLen());
    switch (event.getType()) {
        case WRITE_ENABLE:
        case WRITE_DISABLE:
        case BULK_ERASE:
        case DEEP_POWER_DOWN_CMD:
        case RELEASE_POWER_DOWN:
            return clocks_to_us(8, cfg.f_c_mhz);
        case READ_STATUS:
            return clocks_to_us(8 + 8 * max(1, len), cfg.f_c_mhz);
        case WRITE_STATUS:
            return clocks_to_us(16, cfg.f_c_mhz);
        case READ:
            return clocks_to_us(32 + 8 * len, cfg.f_r_mhz);
        case FAST_READ:
            return clocks_to_us(40 + 8 * len, cfg.f_c_mhz);
        case PAGE_PROGRAM:
            return clocks_to_us(32 + 8 * len, cfg.f_c_mhz);
        case SECTOR_ERASE:
            return clocks_to_us(32, cfg.f_c_mhz);
        case READ_ID:
            return clocks_to_us(8 + 8 * max(1, len), cfg.f_c_mhz);
        case READ_ELECTRONIC_SIGNATURE:
            return clocks_to_us(32 + 8 * max(0, len), cfg.f_c_mhz);
        default:
            return 0.0;
    }
}

void FlashCore::add_time(double us) {
    current_operation_time += us;
    complete_pending_if_ready();
    chip.time = current_operation_time;
}

void FlashCore::read_bytes(int addr, uint8_t* buf, int len) {
    if (buf == nullptr || len <= 0) return;
    storage_file.clear();
    for (int i = 0; i < len; i++) {
        int a = normalize_addr(addr + i);
        storage_file.seekg(a);
        char ch = (char)0xFF;
        storage_file.read(&ch, 1);
        buf[i] = (uint8_t)ch;
    }
}

void FlashCore::read_status(uint8_t* buf, int len) {
    if (buf == nullptr || len <= 0) return;
    for (int i = 0; i < len; i++) {
        buf[i] = chip.status_reg & 0x9F; // b6,b5 固定读 0
    }
}

void FlashCore::read_id(uint8_t* buf, int len) {
    if (buf == nullptr || len <= 0) return;
    uint8_t id[20] = {
        0x20, 0x20, 0x13, 0x10,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    };
    for (int i = 0; i < len; i++) {
        buf[i] = id[i % 20];
    }
}

void FlashCore::read_signature(uint8_t* buf, int len) {
    if (buf == nullptr || len <= 0) return;
    for (int i = 0; i < len; i++) {
        buf[i] = 0x12;
    }
}

bool FlashCore::start_pending(EventType type, int addr, int len,
                              const vector<uint8_t>& data,
                              const vector<uint8_t>& valid,
                              uint8_t sr_value,
                              double bus_time_us,
                              double internal_time_us) {
    if (pending.active) {
        cerr << "[错误] 已有内部周期未完成，命令被拒绝" << endl;
        return false;
    }

    add_time(bus_time_us);

    pending.active = true;
    pending.type = type;
    pending.addr = normalize_addr(addr);
    pending.len = len;
    pending.sr_value = sr_value;
    pending.data = data;
    pending.valid = valid;
    pending.complete_time = current_operation_time + internal_time_us;

    set_wip(true);
    set_wel(false); // 手册说周期完成前某时刻复位；模型中启动即复位
    chip.busy = true;
    chip.state = BUSY;
    chip.time = current_operation_time;

    cout << "[状态] WIP=1，内部周期开始，预计完成时间："
         << fixed << setprecision(3) << pending.complete_time << "us" << endl;

    if (cfg.auto_complete) {
        current_operation_time = pending.complete_time;
        complete_pending_if_ready();
    }
    return true;
}

void FlashCore::complete_pending_if_ready() {
    if (!pending.active) return;
    if (current_operation_time + 1e-9 < pending.complete_time) return;

    switch (pending.type) {
        case PAGE_PROGRAM:
            apply_page_program();
            break;
        case SECTOR_ERASE:
            apply_sector_erase();
            break;
        case BULK_ERASE:
            apply_bulk_erase();
            break;
        case WRITE_STATUS:
            apply_write_status();
            break;
        default:
            break;
    }

    pending = PendingOperation();
    set_wip(false);
    set_wel(false);
    chip.busy = false;
    chip.state = chip.deep_power_down ? DEEP_POWER_DOWN : IDLE;
    chip.time = current_operation_time;
    cout << "[状态] 内部周期完成，WIP=0，当前时间："
         << fixed << setprecision(3) << current_operation_time << "us" << endl;
}

void FlashCore::apply_page_program() {
    int sector, page, offset_dummy;
    parse_address(pending.addr, sector, page, offset_dummy);
    int page_base = normalize_addr(pending.addr) & ~(cfg.page_size - 1);

    storage_file.clear();
    for (int i = 0; i < cfg.page_size; i++) {
        if (pending.valid.empty() || pending.valid[i] == 0) continue;
        int off = page_base + i;
        storage_file.seekg(off);
        char old_ch = (char)0xFF;
        storage_file.read(&old_ch, 1);
        uint8_t new_ch = ((uint8_t)old_ch) & pending.data[i]; // NOR 只允许 1 -> 0
        storage_file.seekp(off);
        storage_file.write((char*)&new_ch, 1);
        chip.sectors[sector]->pages[page]->data[i] = new_ch;
    }
    storage_file.flush();
    update_page_status(sector, page);
    cout << "[硬件(M25P40)] Page Program 完成：Sector=" << sector
         << ", Page=" << page << endl;
}

void FlashCore::apply_sector_erase() {
    int sector, page, offset;
    parse_address(pending.addr, sector, page, offset);
    int base = sector * sector_size();
    vector<uint8_t> erase_data(cfg.page_size, 0xFF);

    storage_file.clear();
    for (int p = 0; p < cfg.page_per_sector; p++) {
        int off = base + p * cfg.page_size;
        storage_file.seekp(off);
        storage_file.write((char*)erase_data.data(), cfg.page_size);
    }
    storage_file.flush();
    chip.sectors[sector]->erase();
    cout << "[硬件(M25P40)] Sector Erase 完成：Sector=" << sector << endl;
}

void FlashCore::apply_bulk_erase() {
    vector<uint8_t> erase_data(cfg.page_size, 0xFF);
    storage_file.clear();
    storage_file.seekp(0);
    for (int i = 0; i < cfg.total_pages; i++) {
        storage_file.write((char*)erase_data.data(), cfg.page_size);
    }
    storage_file.flush();
    for (int s = 0; s < cfg.sector_count; s++) {
        chip.sectors[s]->erase();
    }
    cout << "[硬件(M25P40)] Bulk Erase 完成：全片 0xFF" << endl;
}

void FlashCore::apply_write_status() {
    uint8_t keep = chip.status_reg & (SR_WIP | SR_WEL);
    uint8_t writable = pending.sr_value & (SR_SRWD | SR_BP2 | SR_BP1 | SR_BP0);
    chip.status_reg = keep | writable;
    cout << "[硬件(M25P40)] 写状态寄存器完成，SR=0x"
         << hex << uppercase << setw(2) << setfill('0') << (int)(chip.status_reg & 0x9F)
         << dec << nouppercase << setfill(' ') << endl;
}

void FlashCore::update_page_status(int sector, int page) {
    bool all_ff = true;
    int base = (sector * cfg.page_per_sector + page) * cfg.page_size;
    storage_file.clear();
    storage_file.seekg(base);
    for (int i = 0; i < cfg.page_size; i++) {
        char ch = 0x00;
        storage_file.read(&ch, 1);
        chip.sectors[sector]->pages[page]->data[i] = (uint8_t)ch;
        if ((uint8_t)ch != 0xFF) all_ff = false;
    }
    chip.sectors[sector]->pages[page]->status = all_ff ? FREE : VALID;
}

void FlashCore::execute_write_enable() {
    add_time(command_bus_time(FlashEvent(WRITE_ENABLE, 0, current_operation_time, cfg)));
    set_wel(true);
    cout << "[命令] WRITE ENABLE：WEL=1" << endl;
}

void FlashCore::execute_write_disable() {
    add_time(command_bus_time(FlashEvent(WRITE_DISABLE, 0, current_operation_time, cfg)));
    set_wel(false);
    cout << "[命令] WRITE DISABLE：WEL=0" << endl;
}

void FlashCore::execute_read_status(FlashEvent& event) {
    int len = max(1, event.getLen());
    read_status(event.getBuf(), event.getLen());
    add_time(command_bus_time(event));
    if (event.getBuf() != nullptr && event.getLen() > 0) {
        print_data_hex("[数据] Status Register", event.getBuf(), event.getLen());
    }
    cout << "[命令] READ STATUS：SR=0x"
         << hex << uppercase << setw(2) << setfill('0') << (int)(chip.status_reg & 0x9F)
         << dec << nouppercase << setfill(' ') << "，读取字节=" << len << endl;
}

void FlashCore::execute_write_status(FlashEvent& event) {
    if (!is_wel()) {
        cerr << "[警告] WRITE STATUS 被忽略：WEL=0" << endl;
        add_time(command_bus_time(event));
        return;
    }
    if (hardware_protected()) {
        cerr << "[警告] WRITE STATUS 被拒绝：SRWD=1 且 W#=LOW，硬件保护生效" << endl;
        set_wel(false);
        add_time(command_bus_time(event));
        return;
    }
    if (event.getBuf() == nullptr || event.getLen() < 1) {
        cerr << "[错误] WRITE STATUS 缺少状态字节" << endl;
        add_time(command_bus_time(event));
        return;
    }

    uint8_t new_sr = event.getBuf()[0];
    double t = cfg.use_max_time ? cfg.t_w_max_us : cfg.t_w_us;
    start_pending(WRITE_STATUS, 0, 1, vector<uint8_t>(), vector<uint8_t>(), new_sr,
                  command_bus_time(event), t);
}

void FlashCore::execute_read(FlashEvent& event, bool fast) {
    int len = event.getLen();
    if (len <= 0 || event.getBuf() == nullptr) {
        cerr << "[错误] READ 缓冲区为空或长度为 0" << endl;
        add_time(command_bus_time(event));
        return;
    }
    read_bytes(event.getAddr(), event.getBuf(), len);
    add_time(command_bus_time(event));

    int sector, page, offset;
    parse_address(event.getAddr(), sector, page, offset);
    print_data_hex(fast ? "[数据] FAST READ 内容" : "[数据] READ 内容", event.getBuf(), len);
    cout << "[硬件(M25P40)] " << (fast ? "Fast Read" : "Read")
         << "：Addr=0x" << hex << uppercase << setw(6) << setfill('0') << normalize_addr(event.getAddr())
         << dec << nouppercase << setfill(' ')
         << "，Sector=" << sector << "，Page=" << page << "，Offset=" << offset << endl;
}

void FlashCore::execute_page_program(FlashEvent& event) {
    if (!is_wel()) {
        cerr << "[警告] PAGE PROGRAM 被忽略：WEL=0" << endl;
        add_time(command_bus_time(event));
        return;
    }
    if (event.getBuf() == nullptr || event.getLen() <= 0) {
        cerr << "[错误] PAGE PROGRAM 至少需要 1 字节数据" << endl;
        add_time(command_bus_time(event));
        return;
    }

    int sector, page, offset;
    parse_address(event.getAddr(), sector, page, offset);
    if (sector_protected(sector)) {
        cerr << "[警告] PAGE PROGRAM 被拒绝：目标 Sector=" << sector << " 受 BP 保护" << endl;
        set_wel(false);
        add_time(command_bus_time(event));
        return;
    }

    vector<uint8_t> page_buffer(cfg.page_size, 0xFF);
    vector<uint8_t> valid(cfg.page_size, 0);
    int pos = offset;
    for (int i = 0; i < event.getLen(); i++) {
        page_buffer[pos] = event.getBuf()[i];
        valid[pos] = 1;
        pos = (pos + 1) % cfg.page_size; // M25P40 页内回卷，不跨页
    }

    print_data_hex("[数据] PAGE PROGRAM 输入", event.getBuf(), event.getLen());
    start_pending(PAGE_PROGRAM, event.getAddr(), event.getLen(), page_buffer, valid, 0,
                  command_bus_time(event), page_program_time_us(event.getLen()));
}

void FlashCore::execute_sector_erase(FlashEvent& event) {
    if (!is_wel()) {
        cerr << "[警告] SECTOR ERASE 被忽略：WEL=0" << endl;
        add_time(command_bus_time(event));
        return;
    }

    int sector, page, offset;
    parse_address(event.getAddr(), sector, page, offset);
    if (sector_protected(sector) || hardware_protected()) {
        cerr << "[警告] SECTOR ERASE 被拒绝：Sector=" << sector << " 受保护" << endl;
        set_wel(false);
        add_time(command_bus_time(event));
        return;
    }

    double t = cfg.use_max_time ? cfg.t_erase_sector_max_us : cfg.t_erase_sector_us;
    start_pending(SECTOR_ERASE, event.getAddr(), 0, vector<uint8_t>(), vector<uint8_t>(), 0,
                  command_bus_time(event), t);
}

void FlashCore::execute_bulk_erase(FlashEvent& event) {
    if (!is_wel()) {
        cerr << "[警告] BULK ERASE 被忽略：WEL=0" << endl;
        add_time(command_bus_time(event));
        return;
    }
    if (bp_value() != 0 || hardware_protected()) {
        cerr << "[警告] BULK ERASE 被拒绝：BP!=000 或硬件保护生效" << endl;
        set_wel(false);
        add_time(command_bus_time(event));
        return;
    }

    double t = cfg.use_max_time ? cfg.t_erase_bulk_max_us : cfg.t_erase_bulk_us;
    start_pending(BULK_ERASE, 0, 0, vector<uint8_t>(), vector<uint8_t>(), 0,
                  command_bus_time(event), t);
}

void FlashCore::execute_deep_power_down() {
    FlashEvent tmp(DEEP_POWER_DOWN_CMD, 0, current_operation_time, cfg);
    add_time(command_bus_time(tmp) + cfg.t_dpd_us);
    chip.deep_power_down = true;
    chip.state = DEEP_POWER_DOWN;
    cout << "[命令] DEEP POWER-DOWN：进入深度掉电模式" << endl;
}

void FlashCore::execute_release_power_down() {
    FlashEvent tmp(RELEASE_POWER_DOWN, 0, current_operation_time, cfg);
    add_time(command_bus_time(tmp) + cfg.t_res_us);
    chip.deep_power_down = false;
    chip.state = IDLE;
    cout << "[命令] RELEASE POWER-DOWN：退出深度掉电模式" << endl;
}

void FlashCore::execute_read_id(FlashEvent& event) {
    read_id(event.getBuf(), event.getLen());
    add_time(command_bus_time(event));
    if (event.getBuf() != nullptr && event.getLen() > 0) {
        print_data_hex("[数据] READ ID", event.getBuf(), event.getLen());
    }
    cout << "[命令] READ ID：Manufacturer=0x20, MemoryType=0x20, Capacity=0x13" << endl;
}

void FlashCore::execute_read_signature(FlashEvent& event) {
    bool was_dpd = chip.deep_power_down;
    read_signature(event.getBuf(), event.getLen());
    add_time(command_bus_time(event) + (was_dpd ? cfg.t_res_us : 0.0));
    if (was_dpd) {
        chip.deep_power_down = false;
        chip.state = IDLE;
    }
    if (event.getBuf() != nullptr && event.getLen() > 0) {
        print_data_hex("[数据] READ ELECTRONIC SIGNATURE", event.getBuf(), event.getLen());
    }
    cout << "[命令] READ ELECTRONIC SIGNATURE：RES=0x12" << endl;
}

void FlashCore::execute_wait(FlashEvent& event) {
    add_time(event.getWaitUs());
    cout << "[时间] WAIT " << fixed << setprecision(3) << event.getWaitUs()
         << "us，当前时间：" << current_operation_time << "us" << endl;
}

void FlashCore::execute(FlashEvent& event) {
    complete_pending_if_ready();

    if (event.getType() == WAIT) {
        execute_wait(event);
        return;
    }

    // 深度掉电模式：只接受 Release 与 Read Electronic Signature
    if (chip.deep_power_down &&
        event.getType() != RELEASE_POWER_DOWN &&
        event.getType() != READ_ELECTRONIC_SIGNATURE) {
        cerr << "[警告] 当前处于 Deep Power-Down，命令被忽略" << endl;
        add_time(command_bus_time(event));
        return;
    }

    // WIP=1 时，只有 RDSR 可轮询；其他存储阵列/修改命令被拒绝，不影响内部周期
    if (is_wip() && event.getType() != READ_STATUS) {
        cerr << "[警告] WIP=1，命令被拒绝，不影响正在进行的内部周期" << endl;
        add_time(command_bus_time(event));
        return;
    }

    switch (event.getType()) {
        case WRITE_ENABLE:
            execute_write_enable();
            break;
        case WRITE_DISABLE:
            execute_write_disable();
            break;
        case READ_STATUS:
            execute_read_status(event);
            break;
        case WRITE_STATUS:
            execute_write_status(event);
            break;
        case READ:
            execute_read(event, false);
            break;
        case FAST_READ:
            execute_read(event, true);
            break;
        case PAGE_PROGRAM:
            execute_page_program(event);
            break;
        case SECTOR_ERASE:
            execute_sector_erase(event);
            break;
        case BULK_ERASE:
            execute_bulk_erase(event);
            break;
        case DEEP_POWER_DOWN_CMD:
            execute_deep_power_down();
            break;
        case RELEASE_POWER_DOWN:
            execute_release_power_down();
            break;
        case READ_ID:
            execute_read_id(event);
            break;
        case READ_ELECTRONIC_SIGNATURE:
            execute_read_signature(event);
            break;
        case WAIT:
            execute_wait(event);
            break;
        default:
            cerr << "[错误] 未知命令" << endl;
            break;
    }

    chip.time = current_operation_time;
}

double FlashCore::get_time() const {
    return current_operation_time;
}

bool FlashCore::is_busy() const {
    return chip.busy;
}

uint8_t FlashCore::get_status() const {
    return chip.status_reg & 0x9F;
}

void FlashCore::set_write_protect(bool low) {
    chip.write_protect_low = low;
}
