#include "TextLayout.hpp"

#include <algorithm>
#include <cstddef>
#include <format>
#include <utility>

#include <ftxui/screen/string.hpp>

namespace file_monitor::tui {
    namespace {

        constexpr std::string_view ELLIPSIS{ "..." };
        constexpr std::string_view RENAME_SEPARATOR{ " -> " };
        constexpr auto ELLIPSIS_WIDTH{ static_cast<int>(ELLIPSIS.size()) };
        constexpr auto RENAME_SEPARATOR_WIDTH{ static_cast<int>(RENAME_SEPARATOR.size()) };

        // One entry per terminal column; a full-width glyph is followed by an
        // empty entry for its second column.
        using Cells = std::vector<std::string>;

        auto is_separator(std::string_view cell) -> bool {
            return cell == "\\" || cell == "/";
        }

        auto join_cells(Cells const& cells, std::size_t first, std::size_t last) -> std::string {
            std::string text;
            for (auto index{ first }; index < last; ++index) {
                text += cells[index];
            }
            return text;
        }

        // Moves a cut that would split a full-width glyph past that glyph.
        auto skip_glyph_tail(Cells const& cells, std::size_t index) -> std::size_t {
            while (index < cells.size() && cells[index].empty()) {
                ++index;
            }
            return index;
        }

        // Moves a cut that would split a full-width glyph before that glyph.
        auto before_glyph_tail(Cells const& cells, std::size_t index) -> std::size_t {
            while (index > 0 && index < cells.size() && cells[index].empty()) {
                --index;
            }
            return index;
        }

        // The first cell of the longest end of `cells` that fits in `width`.
        auto tail_start(Cells const& cells, int width) -> std::size_t {
            auto const width_cells{ static_cast<std::size_t>(std::max(width, 0)) };
            if (width_cells >= cells.size()) {
                return 0;
            }
            return skip_glyph_tail(cells, cells.size() - width_cells);
        }

        // Splits "D:\a\b.txt" into "D:\a\" and "b.txt". A directory keeps its
        // trailing separator in the name: "D:\a\b\" gives "D:\a\" and "b\".
        auto split_parent(std::string_view path) -> std::pair<std::string_view, std::string_view> {
            auto const name_end{
                path.ends_with('\\') || path.ends_with('/') ? path.size() - 1 : path.size()
            };
            if (name_end == 0) {
                return { {}, path };
            }
            auto const separator{ path.find_last_of("\\/", name_end - 1) };
            if (separator == std::string_view::npos) {
                return { {}, path };
            }
            return { path.substr(0, separator + 1), path.substr(separator + 1) };
        }

    } // namespace

    auto shorten_path(std::string_view path, int width) -> std::string {
        auto const cells{ ftxui::Utf8ToGlyphs(path) };
        if (static_cast<int>(cells.size()) <= width) {
            return std::string{ path };
        }
        if (width <= ELLIPSIS_WIDTH) {
            return std::string{ ELLIPSIS.substr(0, static_cast<std::size_t>(std::max(width, 0))) };
        }

        // The head is the drive and the first directory, such as "D:\proj\".
        std::size_t head_end{ 0 };
        auto separator_count{ 0 };
        for (auto index{ std::size_t{ 0 } }; index < cells.size(); ++index) {
            if (is_separator(cells[index]) && ++separator_count == 2) {
                head_end = index + 1;
                break;
            }
        }

        auto const tail_width{ width - static_cast<int>(head_end) - ELLIPSIS_WIDTH };
        if (head_end > 0 && tail_width > 0) {
            // Start the tail at a separator rather than inside a name; the
            // trailing separator of a directory does not count.
            for (auto index{ tail_start(cells, tail_width) }; index + 1 < cells.size(); ++index) {
                if (is_separator(cells[index])) {
                    return join_cells(cells, 0, head_end) +
                        std::string{ ELLIPSIS } +
                        join_cells(cells, index, cells.size());
                }
            }
        }

        // Not even the last name fits next to the head; keep the end of it.
        return std::string{ ELLIPSIS } +
            join_cells(cells, tail_start(cells, width - ELLIPSIS_WIDTH), cells.size());
    }

    auto wrap_text(std::string_view text, int width) -> std::vector<std::string> {
        auto const cells{ ftxui::Utf8ToGlyphs(text) };
        auto const line_width{ static_cast<std::size_t>(std::max(width, 1)) };

        std::vector<std::string> lines;
        std::size_t line_start{ 0 };
        while (cells.size() - line_start > line_width) {
            auto line_end{ before_glyph_tail(cells, line_start + line_width) };
            // Break after the last separator of the line if that keeps at
            // least half of the line.
            for (auto index{ line_end }; index > line_start + line_width / 2; --index) {
                if (is_separator(cells[index - 1])) {
                    line_end = index;
                    break;
                }
            }
            if (line_end <= line_start) {
                // A full-width glyph wider than the line.
                line_end = skip_glyph_tail(cells, line_start + 1);
            }
            lines.push_back(join_cells(cells, line_start, line_end));
            line_start = line_end;
        }
        lines.push_back(join_cells(cells, line_start, cells.size()));
        return lines;
    }

    auto change_path_text(core::FileChange const& change, int width) -> std::string {
        if (change.previous_absolute_path.empty()) {
            return shorten_path(change.absolute_path, width);
        }

        std::string_view const previous{ change.previous_absolute_path };
        std::string_view current{ change.absolute_path };
        auto const [previous_parent, previous_name] = split_parent(previous);
        auto const [parent, name] = split_parent(current);
        if (!parent.empty() && parent == previous_parent) {
            current = name;
        }

        // Share the width between both paths, giving a short one all it needs.
        auto const available_width{ std::max(width - RENAME_SEPARATOR_WIDTH, 0) };
        auto previous_width{ ftxui::string_width(previous) };
        auto current_width{ ftxui::string_width(current) };
        if (previous_width + current_width > available_width) {
            auto const half_width{ available_width / 2 };
            if (current_width <= half_width) {
                previous_width = available_width - current_width;
            } else if (previous_width <= half_width) {
                current_width = available_width - previous_width;
            } else {
                previous_width = available_width - half_width;
                current_width = half_width;
            }
        }
        return std::format(
            "{}{}{}",
            shorten_path(previous, previous_width),
            RENAME_SEPARATOR,
            shorten_path(current, current_width)
        );
    }

    auto time_of_day(std::string_view time) -> std::string_view {
        auto const separator{ time.find(' ') };
        if (separator == std::string_view::npos) {
            return time;
        }
        return time.substr(separator + 1);
    }

} // namespace file_monitor::tui
