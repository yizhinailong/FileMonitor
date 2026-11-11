#include "Shell.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <format>
#include <string>
#include <system_error>
#include <utility>

#ifndef NOMINMAX
    #define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <shellapi.h>

namespace file_monitor::tui {
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

        auto run_explorer(std::wstring const& arguments) -> bool {
            auto const result{
                ShellExecuteW(nullptr, L"open", L"explorer.exe", arguments.c_str(), nullptr, SW_SHOWNORMAL)
            };
            // ShellExecuteW returns a value above 32 on success.
            return reinterpret_cast<std::intptr_t>(result) > 32;
        }

        auto path_exists(std::filesystem::path const& path) -> bool {
            std::error_code error;
            return std::filesystem::exists(path, error);
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

    auto show_in_explorer(std::string_view path_text) -> bool {
        std::filesystem::path path{ to_wide(path_text) };
        // Directories in the change list end with a separator.
        if (!path.has_filename()) {
            path = path.parent_path();
        }
        if (path_exists(path)) {
            return run_explorer(std::format(L"/select,\"{}\"", path.native()));
        }

        auto directory{ path.parent_path() };
        while (!path_exists(directory)) {
            auto parent{ directory.parent_path() };
            if (parent == directory) {
                return false;
            }
            directory = std::move(parent);
        }
        return run_explorer(std::format(L"\"{}\"", directory.native()));
    }

} // namespace file_monitor::tui
