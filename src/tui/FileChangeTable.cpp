#include "FileChangeTable.hpp"

#include <algorithm>
#include <format>
#include <ranges>
#include <span>
#include <string>
#include <string_view>

#include <ftxui/component/mouse.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/string.hpp>
#include <ftxui/screen/terminal.hpp>

#include "core/FileSystem.hpp"

namespace file_monitor::tui {
    namespace {

        constexpr auto COLUMN_GAP{ 2 };
        constexpr auto HORIZONTAL_SCROLL_STEP{ 8 };
        constexpr auto WHEEL_SCROLL_STEP{ std::ptrdiff_t{ 3 } };
        constexpr std::string_view RENAME_SEPARATOR{ " -> " };

        struct ColumnWidths {
            int time{ 0 };
            int status{ 0 };
            int size{ 0 };
            int path{ 0 };
        };

        auto file_change_status_color(core::FileChangeStatus status) -> ftxui::Color {
            switch (status) {
                case core::FileChangeStatus::Added:
                    return ftxui::Color::Green;
                case core::FileChangeStatus::Removed:
                    return ftxui::Color::Red;
                case core::FileChangeStatus::Modified:
                    return ftxui::Color::Yellow;
                case core::FileChangeStatus::Renamed:
                case core::FileChangeStatus::Moved:
                    return ftxui::Color::Yellow;
            }
            return ftxui::Color::Default;
        }

        auto file_change_path_text(core::FileChange const& change) -> std::string {
            if (change.previous_absolute_path.empty()) {
                return change.absolute_path;
            }
            return std::format(
                "{}{}{}",
                change.previous_absolute_path,
                RENAME_SEPARATOR,
                change.absolute_path
            );
        }

        auto file_change_path_width(core::FileChange const& change) -> int {
            auto width{ ftxui::string_width(change.absolute_path) };
            if (!change.previous_absolute_path.empty()) {
                width += ftxui::string_width(change.previous_absolute_path) +
                    ftxui::string_width(RENAME_SEPARATOR);
            }
            return width;
        }

        auto measure_columns(std::span<core::FileChange const> changes) -> ColumnWidths {
            ColumnWidths widths{
                .time = ftxui::string_width("时间"),
                .status = ftxui::string_width("状态"),
                .size = ftxui::string_width("大小"),
                .path = ftxui::string_width("绝对路径")
            };
            for (auto const& change : changes) {
                widths.time = std::max(widths.time, ftxui::string_width(change.time));
                widths.status = std::max(
                    widths.status,
                    ftxui::string_width(core::file_change_status_text(change.status))
                );
                widths.size = std::max(widths.size, ftxui::string_width(change.size));
                widths.path = std::max(widths.path, file_change_path_width(change));
            }
            return widths;
        }

        auto skip_columns(std::string_view text, int column_count) -> std::string {
            if (column_count <= 0) {
                return std::string{ text };
            }
            return ftxui::Utf8ToGlyphs(text) |
                std::views::drop(column_count) |
                std::views::join |
                std::ranges::to<std::string>();
        }

        auto make_row(
            ftxui::Element time,
            ftxui::Element status,
            ftxui::Element size,
            ftxui::Element path,
            ColumnWidths const& widths
        ) -> ftxui::Element {
            auto const fixed_width{ [](int width) {
                return ftxui::size(ftxui::WIDTH, ftxui::EQUAL, width + COLUMN_GAP);
            } };
            return ftxui::hbox({
                std::move(time) | fixed_width(widths.time),
                std::move(status) | fixed_width(widths.status),
                std::move(size) | fixed_width(widths.size),
                std::move(path) | ftxui::xflex,
            });
        }

    } // namespace

    FileChangeTable::FileChangeTable(core::MonitorSession const& session)
        : m_session{ session } {
    }

    auto FileChangeTable::Render() -> ftxui::Element {
        auto const changes{ m_session.Changes() };
        auto const widths{ measure_columns(changes) };

        auto const last_first_row{ lastFirstRow() };
        m_first_row = m_follow_latest ? last_first_row : std::min(m_first_row, last_first_row);

        auto const fixed_columns_width{ widths.time + widths.status + widths.size + 3 * COLUMN_GAP };
        auto const path_column_width{ m_body_box.x_max - m_body_box.x_min + 1 - fixed_columns_width };
        m_first_column = std::clamp(m_first_column, 0, std::max(0, widths.path - path_column_width));

        // The body box is only known after layout, so render enough rows to fill
        // the terminal and let the frame clip them; this keeps the view correct
        // right after the terminal is resized.
        auto const rendered_row_count{
            std::max(
                visibleRowCount(),
                static_cast<std::size_t>(std::max(ftxui::Terminal::Size().dimy, 0))
            )
        };
        auto first_rendered_row{ m_first_row };
        if (m_follow_latest) {
            first_rendered_row = changes.size() > rendered_row_count ?
                changes.size() - rendered_row_count :
                0;
        }
        auto const rendered_changes{
            changes.subspan(
                first_rendered_row,
                std::min(rendered_row_count, changes.size() - first_rendered_row)
            )
        };

        ftxui::Elements rows;
        rows.reserve(rendered_changes.size());
        for (auto const& change : rendered_changes) {
            rows.push_back(
                make_row(
                    ftxui::text(change.time),
                    ftxui::text(core::file_change_status_text(change.status)) |
                        ftxui::color(file_change_status_color(change.status)),
                    ftxui::text(change.size),
                    ftxui::text(skip_columns(file_change_path_text(change), m_first_column)),
                    widths
                )
            );
        }
        if (m_follow_latest && !rows.empty()) {
            rows.back() |= ftxui::focus;
        }

        auto header{
            make_row(
                ftxui::text("时间"),
                ftxui::text("状态"),
                ftxui::text("大小"),
                ftxui::text("绝对路径"),
                widths
            ) |
            ftxui::bold
        };
        return ftxui::vbox({
            std::move(header),
            ftxui::separatorLight(),
            ftxui::vbox(std::move(rows)) | ftxui::yframe | ftxui::flex | ftxui::reflect(m_body_box),
        });
    }

    auto FileChangeTable::OnEvent(ftxui::Event event) -> bool {
        auto const page_row_count{
            static_cast<std::ptrdiff_t>(std::max(visibleRowCount(), std::size_t{ 1 }))
        };
        if (event == ftxui::Event::ArrowUp) {
            scrollRows(-1);
        } else if (event == ftxui::Event::ArrowDown) {
            scrollRows(1);
        } else if (event == ftxui::Event::PageUp) {
            scrollRows(-page_row_count);
        } else if (event == ftxui::Event::PageDown) {
            scrollRows(page_row_count);
        } else if (event == ftxui::Event::Home) {
            m_first_row = 0;
            m_follow_latest = lastFirstRow() == 0;
        } else if (event == ftxui::Event::End) {
            ScrollToLatest();
        } else if (event == ftxui::Event::ArrowLeft) {
            scrollColumns(-HORIZONTAL_SCROLL_STEP);
        } else if (event == ftxui::Event::ArrowRight) {
            scrollColumns(HORIZONTAL_SCROLL_STEP);
        } else if (!event.is_mouse()) {
            return false;
        } else {
            switch (event.mouse().button) {
                case ftxui::Mouse::WheelUp:
                    scrollRows(-WHEEL_SCROLL_STEP);
                    break;
                case ftxui::Mouse::WheelDown:
                    scrollRows(WHEEL_SCROLL_STEP);
                    break;
                case ftxui::Mouse::WheelLeft:
                    scrollColumns(-HORIZONTAL_SCROLL_STEP);
                    break;
                case ftxui::Mouse::WheelRight:
                    scrollColumns(HORIZONTAL_SCROLL_STEP);
                    break;
                default:
                    return false;
            }
        }
        return true;
    }

    auto FileChangeTable::ScrollToLatest() -> void {
        m_follow_latest = true;
    }

    auto FileChangeTable::visibleRowCount() const -> std::size_t {
        if (m_body_box.IsEmpty()) {
            return static_cast<std::size_t>(std::max(ftxui::Terminal::Size().dimy, 0));
        }
        return static_cast<std::size_t>(m_body_box.y_max - m_body_box.y_min + 1);
    }

    auto FileChangeTable::lastFirstRow() const -> std::size_t {
        auto const change_count{ m_session.Changes().size() };
        auto const row_count{ visibleRowCount() };
        return change_count > row_count ? change_count - row_count : 0;
    }

    auto FileChangeTable::scrollRows(std::ptrdiff_t row_count) -> void {
        auto const last_first_row{ static_cast<std::ptrdiff_t>(lastFirstRow()) };
        auto const current_first_row{
            m_follow_latest ?
                last_first_row :
                std::min(static_cast<std::ptrdiff_t>(m_first_row), last_first_row)
        };
        auto const first_row{
            std::clamp(current_first_row + row_count, std::ptrdiff_t{ 0 }, last_first_row)
        };
        m_first_row = static_cast<std::size_t>(first_row);
        m_follow_latest = first_row == last_first_row;
    }

    auto FileChangeTable::scrollColumns(int column_count) -> void {
        m_first_column = std::max(0, m_first_column + column_count);
    }

} // namespace file_monitor::tui
