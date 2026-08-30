#pragma once

#include <filesystem>

#include "Window.hpp"
#include "ui/utils/List.hpp"
#include "ui/utils/Text.hpp"

namespace file_monitor::ui::windows {

    class LogWindow final : public Window {
    public:
        LogWindow();

        auto SetDirectory(std::filesystem::path const& directory) -> void;

    private:
        auto renderContent() -> void override;

        utils::Text m_summary_text{ "Select a directory to list its files" };
        utils::List m_file_list{ "##Files" };
    };

} // namespace file_monitor::ui::windows
