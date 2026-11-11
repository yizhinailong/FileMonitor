#include "ChangeFilter.hpp"

#include <algorithm>
#include <cstddef>
#include <ranges>
#include <utility>

namespace file_monitor::core {
    namespace {

        auto status_index(FileChangeStatus status) -> std::size_t {
            return static_cast<std::size_t>(std::to_underlying(status));
        }

        auto ascii_lower(char character) -> char {
            if (character >= 'A' && character <= 'Z') {
                return static_cast<char>(character - 'A' + 'a');
            }
            return character;
        }

        auto contains_ignoring_case(std::string_view text, std::string_view lowercase_part) -> bool {
            return lowercase_part.empty() ||
                !std::ranges::search(text, lowercase_part, {}, ascii_lower).empty();
        }

    } // namespace

    auto ChangeFilter::ToggleStatus(FileChangeStatus status) -> void {
        auto& hidden{ m_hidden_statuses[status_index(status)] };
        hidden = !hidden;
    }

    auto ChangeFilter::ShowsStatus(FileChangeStatus status) const -> bool {
        return !m_hidden_statuses[status_index(status)];
    }

    auto ChangeFilter::SetPathText(std::string_view text) -> void {
        m_path_text = text |
            std::views::transform(ascii_lower) |
            std::ranges::to<std::string>();
    }

    auto ChangeFilter::Clear() -> void {
        m_hidden_statuses.fill(false);
        m_path_text.clear();
    }

    auto ChangeFilter::IsActive() const -> bool {
        return !m_path_text.empty() || std::ranges::contains(m_hidden_statuses, true);
    }

    auto ChangeFilter::Matches(FileChange const& change) const -> bool {
        if (!ShowsStatus(change.status)) {
            return false;
        }
        return contains_ignoring_case(change.absolute_path, m_path_text) ||
            contains_ignoring_case(change.previous_absolute_path, m_path_text);
    }

} // namespace file_monitor::core
