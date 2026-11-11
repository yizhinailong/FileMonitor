#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>

#include "core/ChangeFilter.hpp"
#include "core/FileSystem.hpp"
#include "core/MonitorSession.hpp"

namespace file_monitor::tui {

    // The change table of the main view and the details of its selected
    // change. While the list follows new changes, the latest one is selected.
    // The selection and the first visible row are kept by sequence number, so
    // they stay on the same changes when the session drops older ones.
    class ChangeList final {
    public:
        explicit ChangeList(core::MonitorSession const& session);

        auto Render(int width) -> ftxui::Element;
        // The details of the selected change below a separator, or an empty
        // element when nothing is selected or the details are hidden. Call
        // after Render().
        auto RenderDetails(int width) const -> ftxui::Element;
        auto OnEvent(ftxui::Event event) -> bool;

        auto FollowLatest() -> void;
        auto ToggleDetails() -> void;

        // Filters, see core::ChangeFilter.
        auto ToggleStatus(core::FileChangeStatus status) -> void;
        [[nodiscard]]
        auto ShowsStatus(core::FileChangeStatus status) const -> bool;
        auto SetPathFilter(std::string_view text) -> void;
        auto ClearFilters() -> void;
        [[nodiscard]]
        auto IsFiltered() const -> bool;

        // The selected change, or nullptr when no change matches. Brings the
        // list up to date with the session first.
        [[nodiscard]]
        auto SelectedChange() -> core::FileChange const*;

        // The queries below describe the list as of the last Render().
        [[nodiscard]]
        auto FollowsLatest() const -> bool;
        // Matching changes added since the list stopped following new
        // changes.
        [[nodiscard]]
        auto NewChangeCount() const -> std::size_t;
        // 1-based position of the selected change, or 0 when no change
        // matches.
        [[nodiscard]]
        auto SelectedPosition() const -> std::size_t;
        // The number of changes that match the filters.
        [[nodiscard]]
        auto RowCount() const -> std::size_t;

    private:
        auto refresh() -> void;
        auto selectRow(std::size_t row) -> void;
        auto moveSelection(std::ptrdiff_t row_count) -> void;
        auto scrollToSelection() -> void;
        [[nodiscard]]
        auto selectedChange() const -> core::FileChange const*;
        [[nodiscard]]
        auto visibleRowCount() const -> std::size_t;
        [[nodiscard]]
        auto rowSequence(std::size_t row) const -> std::uint64_t;
        [[nodiscard]]
        auto changeIndex(std::uint64_t sequence) const -> std::size_t;
        [[nodiscard]]
        auto rowAtOrAfter(std::uint64_t sequence) const -> std::size_t;
        [[nodiscard]]
        auto emptyText() const -> std::string_view;

        core::MonitorSession const& m_session;
        // Indices into m_session.Changes() of the changes that match the
        // filters, oldest first.
        std::vector<std::size_t> m_rows;
        std::size_t m_selected_row{ 0 };
        std::size_t m_first_visible_row{ 0 };
        std::uint64_t m_selected_sequence{ 0 };
        std::uint64_t m_first_visible_sequence{ 0 };
        // The sequence number after the last change of the last refresh.
        std::uint64_t m_end_sequence{ 0 };
        // The first sequence number added after the list stopped following.
        std::uint64_t m_unseen_sequence{ 0 };
        bool m_follow_latest{ true };
        bool m_show_details{ true };
        core::ChangeFilter m_filter;
        ftxui::Box m_body_box{ .x_min = 0, .x_max = -1, .y_min = 0, .y_max = -1 };
    };

} // namespace file_monitor::tui
