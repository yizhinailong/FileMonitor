module;

#ifndef NOMINMAX
    #define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <shellapi.h>

export module file_monitor.core:shell;

import std;
import :utils;

namespace file_monitor::core {
    namespace {

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

    // Opens File Explorer with an existing file or directory selected. For a
    // path that no longer exists, opens its closest existing parent directory.
    export auto show_in_explorer(std::string_view path_text) -> bool {
        auto path{ utils::utf8_to_path(path_text) };
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

} // namespace file_monitor::core
