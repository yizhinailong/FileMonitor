#pragma once

#include <array>
#include <string>
#include <string_view>

#include "FileSystem.hpp"

namespace file_monitor::core {

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
