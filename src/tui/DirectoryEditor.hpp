#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

namespace file_monitor::tui {

    class DirectoryEditor final : public ftxui::ComponentBase {
    public:
        DirectoryEditor(
            std::vector<std::filesystem::path>& directories,
            std::string input_hint,
            std::string empty_text
        );

        auto Reset() -> void;
        auto FocusInput() -> void;

        auto OnRender() -> ftxui::Element override;
        auto OnEvent(ftxui::Event event) -> bool override;

    private:
        auto addInputDirectory() -> void;
        auto removeSelectedDirectory() -> void;
        auto clearDirectories() -> void;
        auto refreshEntries() -> void;

        std::vector<std::filesystem::path>& m_directories;
        std::string m_input_hint;
        std::string m_empty_text;
        std::string m_input;
        std::string m_error_message;
        std::vector<std::string> m_entries;
        int m_selected_entry{ 0 };

        ftxui::Component m_input_field;
        ftxui::Component m_add_button;
        ftxui::Component m_remove_button;
        ftxui::Component m_clear_button;
        ftxui::Component m_directory_list;
    };

} // namespace file_monitor::tui
