module;

#include <algorithm>
#include <array>
#include <cstddef>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>

export module file_monitor.core:change_filter;

export import :file_system;

export namespace file_monitor::core {

    // Chooses the changes an interface lists: those whose status is not hidden
    // and whose path contains the path text.
    class ChangeFilter final {
    public:
        // Hides or shows the changes with this status.
        auto ToggleStatus(FileChangeStatus status) -> void;
        [[nodiscard]]
        auto ShowsStatus(FileChangeStatus status) const -> bool;
        // The text has to be a part of either path of a change; the case of
        // ASCII letters is ignored. An empty text matches every change.
        auto SetPathText(std::string_view text) -> void;
        auto Clear() -> void;

        // Whether the filter can hide a change at all.
        [[nodiscard]]
        auto IsActive() const -> bool;
        [[nodiscard]]
        auto Matches(FileChange const& change) const -> bool;

    private:
        std::array<bool, FILE_CHANGE_STATUS_COUNT> m_hidden_statuses{};
        // Lowercase ASCII letters.
        std::string m_path_text;
    };

} // namespace file_monitor::core

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
