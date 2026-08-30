#pragma once

#include "windows/DirectoryWindow.hpp"
#include "windows/LogWindow.hpp"

struct SDL_Window;

namespace file_monitor::ui {

    class MainWindows final {
    public:
        auto SetParentWindow(SDL_Window* window) -> void;
        auto Render() -> void;

    private:
        windows::DirectoryWindow m_directory_window;
        windows::LogWindow       m_log_window;
    };

} // namespace file_monitor::ui
