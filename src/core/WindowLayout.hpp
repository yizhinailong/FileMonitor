#pragma once

#include <filesystem>
#include <optional>

namespace file_monitor::core {

    // The size and position of the desktop window, so that it reopens where
    // the user left it.
    struct WindowLayout {
        int x{ 0 };
        int y{ 0 };
        int width{ 0 };
        int height{ 0 };
        bool maximized{ false };
    };

    // The layout stored in `path`, or nothing when there is none or it cannot
    // be read.
    auto load_window_layout(std::filesystem::path const& path) -> std::optional<WindowLayout>;
    auto save_window_layout(std::filesystem::path const& path, WindowLayout const& layout) -> bool;

} // namespace file_monitor::core
