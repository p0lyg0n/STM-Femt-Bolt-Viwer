#include "log_util.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstdio>
#endif

namespace logc {

void enableVirtualTerminal() {
#ifdef _WIN32
    // Switch the console to UTF-8 so Japanese / Korean strings render
    // correctly alongside ANSI color sequences.
    SetConsoleOutputCP(CP_UTF8);

    auto enableFor = [](DWORD handleId) {
        HANDLE h = GetStdHandle(handleId);
        if(h == INVALID_HANDLE_VALUE || h == nullptr) return;
        DWORD mode = 0;
        if(!GetConsoleMode(h, &mode)) return;
        mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(h, mode);
    };
    enableFor(STD_OUTPUT_HANDLE);
    enableFor(STD_ERROR_HANDLE);
#endif
}

} // namespace logc
