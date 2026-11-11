#include "TextLayout.hpp"

#include <algorithm>
#include <cstddef>

#include <ftxui/screen/string.hpp>

#include "core/PathText.hpp"

namespace file_monitor::tui {
    namespace {

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

        auto column_count(std::string_view text) -> float {
            return static_cast<float>(ftxui::string_width(text));
        }

    } // namespace

    auto shorten_path(std::string_view path, int width) -> std::string {
        return core::shorten_path(path, static_cast<float>(width), column_count);
    }

    auto change_path_text(core::FileChange const& change, int width) -> std::string {
        return core::change_path_text(change, static_cast<float>(width), column_count);
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

} // namespace file_monitor::tui
