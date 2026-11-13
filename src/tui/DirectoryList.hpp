#pragma once

#include <filesystem>
#include <string>
#include <vector>

import ftxui;

namespace file_monitor::tui {

    // A titled list of directories with a selected entry and an optional
    // warning next to each directory.
    class DirectoryList final {
    public:
        DirectoryList(std::vector<std::filesystem::path>& directories, std::string title);

        auto Render(bool focused, int width) -> ftxui::Element;
        // Moves the selection with ↑, ↓, Home, End, and the mouse wheel.
        auto OnEvent(ftxui::Event event) -> bool;
        // Selects the clicked directory; returns whether the click hit the
        // list.
        auto OnClick(int x, int y) -> bool;

        auto Reset() -> void;
        auto SelectLast() -> void;
        auto RemoveSelected() -> void;
        // One entry per directory; an empty entry shows no warning.
        auto SetWarnings(std::vector<std::string> warnings) -> void;

        [[nodiscard]]
        auto Title() const -> std::string const&;
        [[nodiscard]]
        auto Directories() const -> std::vector<std::filesystem::path> const&;

    private:
        std::vector<std::filesystem::path>& m_directories;
        std::string m_title;
        std::vector<std::string> m_warnings;
        int m_selected{ 0 };
        ftxui::Box m_box{ .x_min = 0, .x_max = -1, .y_min = 0, .y_max = -1 };
        ftxui::Box m_rows_box{ .x_min = 0, .x_max = -1, .y_min = 0, .y_max = -1 };
        std::vector<ftxui::Box> m_row_boxes;
    };

} // namespace file_monitor::tui
