#include "DirectoryList.hpp"

#include <algorithm>
#include <cstddef>
#include <format>
#include <ranges>
#include <utility>

#include <ftxui/component/mouse.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/string.hpp>

#include "TextLayout.hpp"

import file_monitor.core.utils;

namespace file_monitor::tui {
    namespace {

        constexpr auto INDENT{ 2 };
        constexpr auto GAP{ 2 };

    } // namespace

    DirectoryList::DirectoryList(std::vector<std::filesystem::path>& directories, std::string title)
        : m_directories{ directories }, m_title{ std::move(title) } {}

    auto DirectoryList::Render(bool focused, int width) -> ftxui::Element {
        auto const count{ static_cast<int>(m_directories.size()) };
        m_selected = std::clamp(m_selected, 0, std::max(count - 1, 0));

        auto title{ ftxui::text(std::format(" {} ({}) ", m_title, count)) | ftxui::bold };
        if (focused) {
            title |= ftxui::inverted;
        }

        ftxui::Elements rows;
        if (m_directories.empty()) {
            rows.push_back(ftxui::text(std::string(INDENT, ' ') + "尚未添加文件夹") | ftxui::dim);
        }
        m_row_boxes.resize(m_directories.size());
        for (auto index{ 0 }; index < count; ++index) {
            auto const row_index{ static_cast<std::size_t>(index) };
            auto const warning{ row_index < m_warnings.size() ? m_warnings[row_index] : std::string{} };
            auto const warning_width{ warning.empty() ? 0 : GAP + ftxui::string_width(warning) };
            auto const path_width{ std::max(width - INDENT - warning_width, 0) };
            auto const path{ core::utils::path_to_utf8(m_directories[row_index]) };

            ftxui::Elements cells{
                ftxui::text(std::string(INDENT, ' ')),
                ftxui::text(shorten_path(path, path_width)) | ftxui::flex,
            };
            if (!warning.empty()) {
                cells.push_back(ftxui::text(std::string(GAP, ' ') + warning) | ftxui::color(ftxui::Color::Yellow));
            }
            auto row{ ftxui::hbox(std::move(cells)) };
            if (focused && index == m_selected) {
                row |= ftxui::inverted;
                row |= ftxui::focus;
            }
            rows.push_back(std::move(row) | ftxui::reflect(m_row_boxes[row_index]));
        }

        auto list{
            ftxui::vbox({
                ftxui::hbox({ std::move(title), ftxui::filler() }),
                ftxui::vbox(std::move(rows)) | ftxui::yframe | ftxui::flex | ftxui::reflect(m_rows_box),
            })
        };
        return std::move(list) | ftxui::reflect(m_box);
    }

    auto DirectoryList::OnEvent(ftxui::Event event) -> bool {
        auto const last{ std::max(static_cast<int>(m_directories.size()) - 1, 0) };
        if (event.is_mouse()) {
            auto const& mouse{ event.mouse() };
            if (!m_box.Contain(mouse.x, mouse.y)) {
                return false;
            }
            if (mouse.button == ftxui::Mouse::WheelUp) {
                m_selected = std::max(m_selected - 1, 0);
                return true;
            }
            if (mouse.button == ftxui::Mouse::WheelDown) {
                m_selected = std::min(m_selected + 1, last);
                return true;
            }
            return false;
        }

        if (event == ftxui::Event::ArrowUp) {
            m_selected = std::max(m_selected - 1, 0);
        } else if (event == ftxui::Event::ArrowDown) {
            m_selected = std::min(m_selected + 1, last);
        } else if (event == ftxui::Event::Home) {
            m_selected = 0;
        } else if (event == ftxui::Event::End) {
            m_selected = last;
        } else {
            return false;
        }
        return true;
    }

    auto DirectoryList::OnClick(int x, int y) -> bool {
        if (!m_box.Contain(x, y)) {
            return false;
        }
        // Rows scrolled out of the frame keep boxes outside of it.
        if (m_rows_box.Contain(x, y)) {
            for (auto const [index, box] : std::views::enumerate(m_row_boxes)) {
                if (box.Contain(x, y)) {
                    m_selected = static_cast<int>(index);
                }
            }
        }
        return true;
    }

    auto DirectoryList::Reset() -> void {
        m_selected = 0;
    }

    auto DirectoryList::SelectLast() -> void {
        m_selected = std::max(static_cast<int>(m_directories.size()) - 1, 0);
    }

    auto DirectoryList::RemoveSelected() -> void {
        if (m_directories.empty()) {
            return;
        }
        auto const index{
            std::clamp(m_selected, 0, static_cast<int>(m_directories.size()) - 1)
        };
        m_directories.erase(m_directories.begin() + index);
        m_selected = std::min(index, std::max(static_cast<int>(m_directories.size()) - 1, 0));
    }

    auto DirectoryList::SetWarnings(std::vector<std::string> warnings) -> void {
        m_warnings = std::move(warnings);
    }

    auto DirectoryList::Title() const -> std::string const& {
        return m_title;
    }

    auto DirectoryList::Directories() const -> std::vector<std::filesystem::path> const& {
        return m_directories;
    }

} // namespace file_monitor::tui
