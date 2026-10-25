// AZSMZ EPaper 2.9 板型板级结构測試。
// 編譯方式見 tests/test_core.py::test_azsmz_board_profile_cpp_contract。

#include "hardware/boards/AzsmzEpaper29.h"
#include "hardware/BoardProfileTypes.h"

#include <cassert>
#include <cstring>

namespace {

using transitink::hardware::kBoardProfileAzsmzEpaper29;

}  // namespace

int main() {
    assert(std::strcmp(kBoardProfileAzsmzEpaper29.id, "azsmz_epaper_29") == 0);
    assert(std::strcmp(kBoardProfileAzsmzEpaper29.displayName, "AZSMZ EPaper 2.9") == 0);
    assert(kBoardProfileAzsmzEpaper29.display.driver ==
           transitink::hardware::DisplayDriverKind::Uc8151);
    assert(kBoardProfileAzsmzEpaper29.display.width == 296);
    assert(kBoardProfileAzsmzEpaper29.display.height == 128);
    assert(kBoardProfileAzsmzEpaper29.display.dcPin == 5);
    assert(kBoardProfileAzsmzEpaper29.display.chipSelectPin == 15);
    assert(kBoardProfileAzsmzEpaper29.display.clockPin == 14);
    assert(kBoardProfileAzsmzEpaper29.display.mosiPin == 13);
    assert(kBoardProfileAzsmzEpaper29.display.resetPin == 2);
    assert(kBoardProfileAzsmzEpaper29.display.busyPin == 4);
    assert(kBoardProfileAzsmzEpaper29.display.powerPin == -1);

    return 0;
}
