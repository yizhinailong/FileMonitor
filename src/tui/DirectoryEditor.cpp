#include "DirectoryEditor.hpp"

#include <algorithm>
#include <ranges>
#include <utility>

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>

#include "Widgets.hpp"
#include "core/Configuration.hpp"
#include "core/Utils.hpp"

namespace file_monitor::tui {

    DirectoryEditor::DirectoryEditor(
        std::vector<std::filesystem::path>& directories,
        std::string input_hint,
        std::string empty_text
    ) : m_directories{ directories }, m_input_hint{ std::move(input_hint) }, m_empty_text{ std::move(empty_text) } {
        ftxui::InputOption input_option;
        input_option.multiline = false;
        input_option.on_change = [this] {
            m_error_message.clear();
        };
        input_option.on_enter = [this] {
            addInputDirectory();
        };
        m_input_field = ftxui::Input(&m_input, &m_input_hint, std::move(input_option));

        auto const has_directories{ [this] { return !m_directories.empty(); } };
        m_add_button = make_button("添加", [this] { addInputDirectory(); });
        m_remove_button = make_button("删除", [this] { removeSelectedDirectory(); }, has_directories);
        m_clear_button = make_button("清空", [this] { clearDirectories(); }, has_directories);
        m_directory_list = ftxui::Menu(&m_entries, &m_selected_entry);

        Add(ftxui::Container::Vertical({
            ftxui::Container::Horizontal({
                m_input_field,
                m_add_button,
                m_remove_button,
                m_clear_button,
            }),
            m_directory_list,
        }));
        refreshEntries();
    }

    auto DirectoryEditor::Reset() -> void {
        m_input.clear();
        m_error_message.clear();
        m_selected_entry = 0;
        refreshEntries();
    }

    auto DirectoryEditor::FocusInput() -> void {
        m_input_field->TakeFocus();
    }

    auto DirectoryEditor::OnRender() -> ftxui::Element {
        auto directory_list{
            m_entries.empty() ?
                ftxui::text(m_empty_text) | ftxui::dim :
                m_directory_list->Render() | ftxui::vscroll_indicator | ftxui::frame
        };
        return ftxui::vbox({
            ftxui::hbox({
                m_input_field->Render() | ftxui::borderLight | ftxui::flex,
                m_add_button->Render(),
                m_remove_button->Render(),
                m_clear_button->Render(),
            }),
            error_text(m_error_message),
            ftxui::text("文件夹路径") | ftxui::bold,
            ftxui::separatorLight(),
            std::move(directory_list) | ftxui::flex,
        });
    }

    auto DirectoryEditor::OnEvent(ftxui::Event event) -> bool {
        if (event == ftxui::Event::Delete && m_directory_list->Focused()) {
            removeSelectedDirectory();
            return true;
        }
        return ComponentBase::OnEvent(std::move(event));
    }

    auto DirectoryEditor::addInputDirectory() -> void {
        auto const result{ core::add_directory_input(m_directories, m_input) };
        if (!result) {
            m_error_message = result.error();
            return;
        }

        m_input.clear();
        m_error_message.clear();
        refreshEntries();
        m_selected_entry = static_cast<int>(m_entries.size()) - 1;
    }

    auto DirectoryEditor::removeSelectedDirectory() -> void {
        if (m_directories.empty()) {
            return;
        }

        auto const last_index{ static_cast<int>(m_directories.size()) - 1 };
        auto const selected_index{ std::clamp(m_selected_entry, 0, last_index) };
        m_directories.erase(m_directories.begin() + selected_index);
        refreshEntries();
        m_selected_entry = std::clamp(selected_index, 0, std::max(0, last_index - 1));
        if (m_entries.empty()) {
            FocusInput();
        }
    }

    auto DirectoryEditor::clearDirectories() -> void {
        m_directories.clear();
        m_selected_entry = 0;
        refreshEntries();
        FocusInput();
    }

    auto DirectoryEditor::refreshEntries() -> void {
        m_entries = m_directories |
            std::views::transform(core::utils::path_to_utf8) |
            std::ranges::to<std::vector<std::string>>();
    }

} // namespace file_monitor::tui
