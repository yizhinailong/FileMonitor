#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <imgui.h>

import file_monitor.core;

namespace file_monitor::gui {

    // What the user asked to do with the selected change.
    enum class ChangeAction {
        None,
        CopyPath,
        CopyPreviousPath,
        ShowInExplorer,
        // Show only the changes in the directory of the change.
        FilterDirectory
    };

    // The table of the changes kept by the session that match a filter, one
    // of which can be selected. Scrolled to the bottom it follows new changes; scrolled
    // elsewhere it keeps its rows in place when the session drops the oldest
    // changes. The selection and the first visible row are remembered by
    // sequence number for that.
    class ChangeTable final {
    public:
        explicit ChangeTable(core::MonitorSession const& session);

        // Changing the filter takes effect with the next Refresh().
        [[nodiscard]] core::ChangeFilter& Filter();
        // Brings the table up to date with the session and the filter. Call
        // once per frame before the functions below.
        void Refresh();
        // Draws the table, which must have rows. A click or the arrow keys
        // select a change; a double click or the context menu of a change
        // ask for an action.
        ChangeAction Render(ImVec2 size);
        // Scrolls to the newest change, which makes the table follow again.
        void ScrollToLatest();

        // The selected change, or nullptr.
        [[nodiscard]] core::FileChange const* SelectedChange() const;
        // The number of changes that match the filter.
        [[nodiscard]] std::size_t RowCount() const;
        [[nodiscard]] bool FollowsLatest() const;
        // The matching changes added since the table stopped following new
        // changes.
        [[nodiscard]] std::size_t NewChangeCount() const;

    private:
        ChangeAction renderRow(std::size_t row);
        void selectRow(std::size_t row);
        [[nodiscard]] std::size_t selectedChangeIndex() const;
        void applySelectionRequests(ImGuiMultiSelectIO const& multi_select);
        void updateFollowing(float scroll_y, float scroll_max_y);
        [[nodiscard]] std::uint64_t rowSequence(std::size_t row) const;
        // The first row with this sequence number or a later one; RowCount()
        // if there is none.
        [[nodiscard]] std::size_t rowAtOrAfter(std::uint64_t sequence) const;

        core::MonitorSession const& m_session;
        core::ChangeFilter m_filter;
        // Indices into m_session.Changes() of the changes that match the
        // filter, oldest first.
        std::vector<std::size_t> m_rows;
        std::optional<std::uint64_t> m_selected_sequence;
        // The sequence number after the last change of the last refresh.
        std::uint64_t m_end_sequence{ 0 };
        // The first sequence number added after the table stopped following.
        std::uint64_t m_unseen_sequence{ 0 };
        bool m_follows_latest{ true };
        bool m_scroll_to_latest{ false };
        // Frames in which a newly selected row is still scrolled into view.
        int m_reveal_frame_count{ 0 };
        // The scroll position of the last frame: the first visible row and how
        // far it is scrolled out of view.
        std::uint64_t m_top_sequence{ 0 };
        float m_top_offset{ 0.0F };
        float m_scroll_y{ 0.0F };
        float m_row_height{ 0.0F };
    };

} // namespace file_monitor::gui
