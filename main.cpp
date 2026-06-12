#include <fstream>
#include <iostream>
#include "config_parser.h"
#include "flash_core.h"
#include "ftl.h"
#include "flash_test.h"

using namespace std;

int main() {
    FlashConfig cfg = load_config("m25p.conf");
    FlashChip chip(cfg);
    FlashCore core(chip, cfg);
    FTL ftl(core, cfg);

    flash_test_run(ftl, cfg);

    ifstream f(cfg.storage_file, ios::binary);
    if (f) cout << "[OK] " << cfg.storage_file << " created!" << endl;
    return 0;
}
