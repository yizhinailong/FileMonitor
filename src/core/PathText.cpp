#include "PathText.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <utility>

namespace file_monitor::core {
    namespace {

        constexpr std::string_view ELLIPSIS{ "..." };
        constexpr std::string_view RENAME_SEPARATOR{ " -> " };

        auto is_separator(char character) -> bool {
            return character == '\\' || character == '/';
        }

        // Whether the byte starts a UTF-8 character, so the text can be cut
        // in front of it.
        auto starts_character(char byte) -> bool {
            return (static_cast<unsigned char>(byte) & 0xC0U) != 0x80U;
        }

        // The longest end of `text` that fits in `width`.
        auto fitting_end(std::string_view text, float width, TextWidth text_width) -> std::string_view {
            for (auto start{ std::size_t{ 0 } }; start < text.size(); ++start) {
                if (starts_character(text[start]) && text_width(text.substr(start)) <= width) {
                    return text.substr(start);
                }
            }
            return {};
        }

        // The end of the head, which is the drive and the first directory,
        // such as "D:\proj\"; 0 for a path without two separators.
        auto head_size(std::string_view path) -> std::size_t {
            auto separator_count{ 0 };
            for (auto index{ std::size_t{ 0 } }; index < path.size(); ++index) {
                if (is_separator(path[index]) && ++separator_count == 2) {
                    return index + 1;
                }
            }
            return 0;
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

    auto shorten_path(std::string_view path, float width, TextWidth text_width) -> std::string {
        if (text_width(path) <= width) {
            return std::string{ path };
        }
        auto const ellipsis_width{ text_width(ELLIPSIS) };
        if (width <= ellipsis_width) {
            auto dots{ ELLIPSIS };
            while (!dots.empty() && text_width(dots) > width) {
                dots.remove_suffix(1);
            }
            return std::string{ dots };
        }

        if (auto const head{ path.substr(0, head_size(path)) }; !head.empty()) {
            auto const end_width{ width - text_width(head) - ellipsis_width };
            // Start the end at a separator rather than inside a name; the
            // trailing separator of a directory does not count.
            for (auto index{ head.size() }; index + 1 < path.size(); ++index) {
                if (is_separator(path[index]) && text_width(path.substr(index)) <= end_width) {
                    return std::format("{}{}{}", head, ELLIPSIS, path.substr(index));
                }
            }
        }

        // Not even the last name fits next to the head; keep the end of it.
        return std::format("{}{}", ELLIPSIS, fitting_end(path, width - ellipsis_width, text_width));
    }

    auto change_path_text(FileChange const& change, float width, TextWidth text_width) -> std::string {
        if (change.previous_absolute_path.empty()) {
            return shorten_path(change.absolute_path, width, text_width);
        }

        std::string_view const previous{ change.previous_absolute_path };
        std::string_view current{ change.absolute_path };
        auto const [previous_parent, previous_name] = split_parent(previous);
        auto const [parent, name] = split_parent(current);
        if (!parent.empty() && parent == previous_parent) {
            current = name;
        }

        // Share the width between both paths, giving a short one all it needs.
        auto const available_width{ std::max(width - text_width(RENAME_SEPARATOR), 0.0F) };
        auto previous_width{ text_width(previous) };
        auto current_width{ text_width(current) };
        if (previous_width + current_width > available_width) {
            auto const half_width{ std::floor(available_width / 2.0F) };
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
            shorten_path(previous, previous_width, text_width),
            RENAME_SEPARATOR,
            shorten_path(current, current_width, text_width)
        );
    }

    auto directory_text(std::string_view path) -> std::string_view {
        auto const separator{ path.find_last_of("\\/") };
        if (separator == std::string_view::npos) {
            return {};
        }
        return path.substr(0, separator + 1);
    }

} // namespace file_monitor::core
