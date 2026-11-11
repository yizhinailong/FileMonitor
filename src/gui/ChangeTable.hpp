#pragma once

#include <cstddef>
#include <cstdint>

#include <imgui.h>

#include "core/FileSystem.hpp"
#include "core/MonitorSession.hpp"

namespace file_monitor::gui {

    // The table of the changes kept by the session. Scrolled to the bottom it
    // follows new changes; scrolled elsewhere it keeps its rows in place when
    // the session drops the oldest changes, by remembering the sequence
    // number of its first visible row.
    class ChangeTable final {
    public:
        explicit ChangeTable(core::MonitorSession const& session);

        // Brings the table up to date with the session. Call once per frame
        // before the functions below.
        auto Refresh() -> void;
        // Draws the table, which must have rows.
        auto Render(ImVec2 size) -> void;
        // Scrolls to the newest change, which makes the table follow again.
        auto ScrollToLatest() -> void;

        [[nodiscard]]
        auto RowCount() const -> std::size_t;
        [[nodiscard]]
        auto FollowsLatest() const -> bool;
        // The changes added since the table stopped following new changes.
        [[nodiscard]]
        auto NewChangeCount() const -> std::size_t;

    private:
        auto renderRow(core::FileChange const& change, std::uint64_t sequence) -> void;
        auto updateFollowing(float scroll_y, float scroll_max_y) -> void;
        [[nodiscard]]
        auto rowAtOrAfter(std::uint64_t sequence) const -> std::size_t;

        core::MonitorSession const& m_session;
        // The sequence number after the last change of the last refresh.
        std::uint64_t m_end_sequence{ 0 };
        // The first sequence number added after the table stopped following.
        std::uint64_t m_unseen_sequence{ 0 };
        bool m_follows_latest{ true };
        bool m_scroll_to_latest{ false };
        // The scroll position of the last frame: the first visible row and how
        // far it is scrolled out of view.
        std::uint64_t m_top_sequence{ 0 };
        float m_top_offset{ 0.0F };
        float m_scroll_y{ 0.0F };
        float m_row_height{ 0.0F };
    };

} // namespace file_monitor::gui
