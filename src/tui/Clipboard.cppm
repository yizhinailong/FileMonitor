module;

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>

#ifndef NOMINMAX
    #define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

export module file_monitor.tui:clipboard;

namespace file_monitor::tui {

    // Puts UTF-8 text on the Windows clipboard of this machine.
    export auto copy_to_clipboard(std::string_view text) -> bool;

    namespace {

        auto to_wide(std::string_view text) -> std::wstring {
            if (text.empty()) {
                return {};
            }
            auto const size{
                MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0)
            };
            std::wstring wide(static_cast<std::size_t>(std::max(size, 0)), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), size);
            return wide;
        }

    } // namespace

    auto copy_to_clipboard(std::string_view text) -> bool {
        auto const wide{ to_wide(text) };
        if (!OpenClipboard(GetConsoleWindow())) {
            return false;
        }

        auto copied{ false };
        auto* const memory{ GlobalAlloc(GMEM_MOVEABLE, (wide.size() + 1) * sizeof(wchar_t)) };
        if (memory != nullptr && EmptyClipboard()) {
            if (auto* const data{ static_cast<wchar_t*>(GlobalLock(memory)) }; data != nullptr) {
                std::ranges::copy(wide, data);
                data[wide.size()] = L'\0';
                GlobalUnlock(memory);
                // The clipboard owns the memory once SetClipboardData succeeds.
                copied = SetClipboardData(CF_UNICODETEXT, memory) != nullptr;
            }
        }
        if (memory != nullptr && !copied) {
            GlobalFree(memory);
        }
        CloseClipboard();
        return copied;
    }

} // namespace file_monitor::tui
