#include "ChangeTable.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <string>
#include <string_view>

#include "Style.hpp"
#include "Widgets.hpp"

import file_monitor.core;

namespace file_monitor::gui {
    namespace {

        constexpr auto COLUMN_COUNT{ 4 };
        constexpr auto TABLE_FLAGS{
            ImGuiTableFlags_Resizable |
            ImGuiTableFlags_ScrollY |
            ImGuiTableFlags_BordersOuter |
            ImGuiTableFlags_BordersInnerV
        };
        // Half a pixel, the precision to which scroll positions are compared.
        constexpr auto SCROLL_TOLERANCE{ 0.5F };
        // Selecting a change can make the details below the table higher in
        // the next frame, which hides the last visible row.
        constexpr auto REVEAL_FRAME_COUNT{ 2 };

        auto full_path_text(core::FileChange const& change) -> std::string {
            if (change.previous_absolute_path.empty()) {
                return change.absolute_path;
            }
            return std::format("{} -> {}", change.previous_absolute_path, change.absolute_path);
        }

    } // namespace

    ChangeTable::ChangeTable(core::MonitorSession const& session)
        : m_session{ session } {}

    auto ChangeTable::Filter() -> core::ChangeFilter& {
        return m_filter;
    }

    auto ChangeTable::Refresh() -> void {
        auto const changes{ m_session.Changes() };
        auto const first_sequence{ m_session.FirstChangeSequence() };
        // None of the changes of the last refresh is left, for example because
        // saving the settings cleared the list: follow the new changes.
        if (first_sequence >= m_end_sequence) {
            m_follows_latest = true;
        }
        m_end_sequence = first_sequence + changes.size();

        m_rows.clear();
        for (auto index{ std::size_t{ 0 } }; index < changes.size(); ++index) {
            if (m_filter.Matches(changes[index])) {
                m_rows.push_back(index);
            }
        }

        if (m_selected_sequence) {
            if (*m_selected_sequence < first_sequence || *m_selected_sequence >= m_end_sequence) {
                // The selected change was dropped.
                m_selected_sequence.reset();
            } else if (!std::ranges::contains(m_rows, selectedChangeIndex())) {
                // A selected change that stops matching, for example because
                // the filter changed, gives its place to the next matching
                // change, or to the last one when none follows.
                auto const row{ rowAtOrAfter(*m_selected_sequence) };
                selectRow(row < m_rows.size() ? row : (m_rows.empty() ? 0 : m_rows.size() - 1));
            }
        }
    }

    auto ChangeTable::Render(ImVec2 size) -> ChangeAction {
        auto const changes{ m_session.Changes() };
        auto const first_sequence{ m_session.FirstChangeSequence() };
        auto const row_count{ m_rows.size() };

        // Dropping the oldest changes moves the remaining rows up. While
        // older changes are being read, scroll to where the first visible row
        // went so it stays in place.
        if (!m_follows_latest && m_row_height > 0.0F && row_count > 0) {
            auto const top_row{
                static_cast<float>(std::min(rowAtOrAfter(m_top_sequence), row_count - 1))
            };
            auto const scroll_y{ top_row * m_row_height + m_top_offset };
            if (std::abs(scroll_y - m_scroll_y) > SCROLL_TOLERANCE) {
                ImGui::SetNextWindowScroll({ -1.0F, scroll_y });
            }
        }

        if (!ImGui::BeginTable("Changes", COLUMN_COUNT, TABLE_FLAGS, size)) {
            return ChangeAction::None;
        }
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("时间", ImGuiTableColumnFlags_WidthFixed, text_width("00:00:00.000"));
        ImGui::TableSetupColumn("状态", ImGuiTableColumnFlags_WidthFixed, text_width("重命名"));
        ImGui::TableSetupColumn("大小", ImGuiTableColumnFlags_WidthFixed, text_width("1023 MB"));
        ImGui::TableSetupColumn("路径", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        updateFollowing(ImGui::GetScrollY(), ImGui::GetScrollMaxY());

        // Dear ImGui turns clicks and the arrow keys into requests to change
        // the selection. It identifies a row by the value given to
        // SetNextItemSelectionUserData(), here the sequence number.
        auto* multi_select{
            ImGui::BeginMultiSelect(
                ImGuiMultiSelectFlags_SingleSelect | ImGuiMultiSelectFlags_ClearOnClickVoid,
                m_selected_sequence ? 1 : 0,
                static_cast<int>(row_count)
            )
        };
        applySelectionRequests(*multi_select);

        // The rows draw the background of the selected one themselves. The
        // frame around the row that has the keyboard focus is left out, as
        // that row is the selected one.
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4{});
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, hovered_row_color());
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, hovered_row_color());
        ImGui::PushStyleColor(ImGuiCol_NavCursor, ImVec4{});
        auto action{ ChangeAction::None };
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(row_count));
        while (clipper.Step()) {
            for (auto row{ clipper.DisplayStart }; row < clipper.DisplayEnd; ++row) {
                auto const row_action{ renderRow(static_cast<std::size_t>(row)) };
                if (row_action != ChangeAction::None) {
                    action = row_action;
                }
            }
        }
        ImGui::PopStyleColor(4);
        m_reveal_frame_count = std::max(m_reveal_frame_count - 1, 0);
        applySelectionRequests(*ImGui::EndMultiSelect());
        if (m_follows_latest) {
            ImGui::SetScrollHereY(1.0F);
        }

        m_scroll_y = ImGui::GetScrollY();
        m_row_height = clipper.ItemsHeight;
        if (m_row_height > 0.0F && row_count > 0) {
            auto const top_row{ std::min(static_cast<std::size_t>(m_scroll_y / m_row_height), row_count - 1) };
            m_top_sequence = rowSequence(top_row);
            m_top_offset = m_scroll_y - static_cast<float>(top_row) * m_row_height;
        }
        ImGui::EndTable();
        return action;
    }

    auto ChangeTable::ScrollToLatest() -> void {
        m_scroll_to_latest = true;
    }

    auto ChangeTable::SelectedChange() const -> core::FileChange const* {
        auto const first_sequence{ m_session.FirstChangeSequence() };
        if (!m_selected_sequence || *m_selected_sequence < first_sequence || *m_selected_sequence >= m_end_sequence) {
            return nullptr;
        }
        auto const index{ selectedChangeIndex() };
        auto const row{ std::ranges::lower_bound(m_rows, index) };
        if (row == m_rows.end() || *row != index) {
            return nullptr;
        }
        return &m_session.Changes()[index];
    }

    auto ChangeTable::RowCount() const -> std::size_t {
        return m_rows.size();
    }

    auto ChangeTable::FollowsLatest() const -> bool {
        return m_follows_latest;
    }

    auto ChangeTable::NewChangeCount() const -> std::size_t {
        if (m_follows_latest) {
            return 0;
        }
        auto const first_sequence{ m_session.FirstChangeSequence() };
        auto const first_unseen{ std::max(m_unseen_sequence, first_sequence) };
        auto const first_row{ std::ranges::lower_bound(m_rows, static_cast<std::size_t>(first_unseen - first_sequence)) };
        return static_cast<std::size_t>(m_rows.end() - first_row);
    }

    auto ChangeTable::renderRow(std::size_t row) -> ChangeAction {
        auto const& change{ m_session.Changes()[m_rows[row]] };
        auto const sequence{ rowSequence(row) };

        auto action{ ChangeAction::None };
        ImGui::PushID(static_cast<int>(sequence));
        ImGui::TableNextRow();
        // Shade every other change rather than every other row, so a row keeps
        // its shade when the changes above it are dropped.
        auto const row_color{ sequence % 2 == 0 ? ImGuiCol_TableRowBg : ImGuiCol_TableRowBgAlt };
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(row_color));

        auto const selected{ m_selected_sequence == sequence };
        if (selected) {
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, ImGui::GetColorU32(selected_row_color()));
        }

        // The row is one selectable item behind the texts of its cells.
        ImGui::TableSetColumnIndex(0);
        ImGui::SetNextItemSelectionUserData(static_cast<ImGuiSelectionUserData>(sequence));
        ImGui::Selectable(
            "##Change",
            selected,
            ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap
        );
        auto const table_bottom{ ImGui::GetWindowPos().y + ImGui::GetWindowHeight() };
        if (selected && m_reveal_frame_count > 0 && ImGui::GetItemRectMax().y > table_bottom) {
            ImGui::SetScrollHereY(1.0F);
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            action = ChangeAction::ShowInExplorer;
        }
        // A right click selects the row before it opens the menu.
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("复制路径", "Ctrl+C")) {
                action = ChangeAction::CopyPath;
            }
            if (!change.previous_absolute_path.empty() && ImGui::MenuItem("复制原路径")) {
                action = ChangeAction::CopyPreviousPath;
            }
            if (ImGui::MenuItem("在资源管理器中显示", "O")) {
                action = ChangeAction::ShowInExplorer;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("只看这个文件夹")) {
                action = ChangeAction::FilterDirectory;
            }
            ImGui::EndPopup();
        }

        ImGui::SameLine(0.0F, 0.0F);
        draw_text(core::file_change_time_of_day(change));
        ImGui::SetItemTooltip("%s", change.time.c_str());

        ImGui::TableSetColumnIndex(1);
        ImGui::PushStyleColor(ImGuiCol_Text, file_change_status_color(change.status));
        draw_text(core::file_change_status_text(change.status));
        ImGui::PopStyleColor();

        ImGui::TableSetColumnIndex(2);
        align_right(text_width(change.size));
        draw_text(change.size);

        ImGui::TableSetColumnIndex(3);
        auto const path_text{
            core::change_path_text(change, ImGui::GetContentRegionAvail().x, text_width)
        };
        draw_text(path_text);
        if (auto const full_text{ full_path_text(change) }; full_text != path_text) {
            ImGui::SetItemTooltip("%s", full_text.c_str());
        }
        ImGui::PopID();
        return action;
    }

    auto ChangeTable::selectRow(std::size_t row) -> void {
        m_selected_sequence = row < m_rows.size() ? std::optional{ rowSequence(row) } : std::nullopt;
    }

    auto ChangeTable::applySelectionRequests(ImGuiMultiSelectIO const& multi_select) -> void {
        for (auto const& request : multi_select.Requests) {
            if (request.Type == ImGuiSelectionRequestType_SetAll) {
                // Selecting all is not possible with a single selection.
                if (!request.Selected) {
                    m_selected_sequence.reset();
                }
            } else if (request.Selected) {
                m_selected_sequence = static_cast<std::uint64_t>(request.RangeFirstItem);
                m_reveal_frame_count = REVEAL_FRAME_COUNT + 1;
            } else if (m_selected_sequence == static_cast<std::uint64_t>(request.RangeFirstItem)) {
                m_selected_sequence.reset();
            }
        }
    }

    auto ChangeTable::updateFollowing(float scroll_y, float scroll_max_y) -> void {
        auto const at_bottom{ scroll_y >= scroll_max_y - SCROLL_TOLERANCE };
        if (m_scroll_to_latest) {
            m_scroll_to_latest = false;
            m_follows_latest = true;
        } else if (!m_follows_latest) {
            m_follows_latest = at_bottom;
        } else if (!at_bottom && scroll_y < m_scroll_y - SCROLL_TOLERANCE) {
            // Only scrolling up stops the table from following. The bottom
            // also moves away when rows are added or the table gets shorter,
            // which leaves the scroll position as it was.
            m_follows_latest = false;
            m_unseen_sequence = m_end_sequence;
        }
    }

    auto ChangeTable::selectedChangeIndex() const -> std::size_t {
        return static_cast<std::size_t>(*m_selected_sequence - m_session.FirstChangeSequence());
    }

    auto ChangeTable::rowSequence(std::size_t row) const -> std::uint64_t {
        return m_session.FirstChangeSequence() + m_rows[row];
    }

    auto ChangeTable::rowAtOrAfter(std::uint64_t sequence) const -> std::size_t {
        auto const first_sequence{ m_session.FirstChangeSequence() };
        if (sequence <= first_sequence) {
            return 0;
        }
        auto const index{ static_cast<std::size_t>(sequence - first_sequence) };
        return static_cast<std::size_t>(std::ranges::lower_bound(m_rows, index) - m_rows.begin());
    }

} // namespace file_monitor::gui
