#include "ChangeTable.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <string>
#include <string_view>

#include "Style.hpp"
#include "Widgets.hpp"
#include "core/PathText.hpp"

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

        auto full_path_text(core::FileChange const& change) -> std::string {
            if (change.previous_absolute_path.empty()) {
                return change.absolute_path;
            }
            return std::format("{} -> {}", change.previous_absolute_path, change.absolute_path);
        }

    } // namespace

    ChangeTable::ChangeTable(core::MonitorSession const& session)
        : m_session{ session } {}

    auto ChangeTable::Refresh() -> void {
        auto const first_sequence{ m_session.FirstChangeSequence() };
        // None of the changes of the last refresh is left, for example because
        // saving the settings cleared the list: follow the new changes.
        if (first_sequence >= m_end_sequence) {
            m_follows_latest = true;
        }
        m_end_sequence = first_sequence + m_session.Changes().size();
    }

    auto ChangeTable::Render(ImVec2 size) -> void {
        auto const changes{ m_session.Changes() };
        auto const first_sequence{ m_session.FirstChangeSequence() };

        // Dropping the oldest changes moves the remaining rows up. While
        // older changes are being read, scroll to where the first visible row
        // went so it stays in place.
        if (!m_follows_latest && m_row_height > 0.0F) {
            auto const top_row{ static_cast<float>(rowAtOrAfter(m_top_sequence)) };
            auto const scroll_y{ top_row * m_row_height + m_top_offset };
            if (std::abs(scroll_y - m_scroll_y) > SCROLL_TOLERANCE) {
                ImGui::SetNextWindowScroll({ -1.0F, scroll_y });
            }
        }

        if (!ImGui::BeginTable("Changes", COLUMN_COUNT, TABLE_FLAGS, size)) {
            return;
        }
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("时间", ImGuiTableColumnFlags_WidthFixed, text_width("00:00:00.000"));
        ImGui::TableSetupColumn("状态", ImGuiTableColumnFlags_WidthFixed, text_width("重命名"));
        ImGui::TableSetupColumn("大小", ImGuiTableColumnFlags_WidthFixed, text_width("1023 MB"));
        ImGui::TableSetupColumn("路径", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        updateFollowing(ImGui::GetScrollY(), ImGui::GetScrollMaxY());

        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(changes.size()));
        while (clipper.Step()) {
            for (auto row{ clipper.DisplayStart }; row < clipper.DisplayEnd; ++row) {
                auto const index{ static_cast<std::size_t>(row) };
                renderRow(changes[index], first_sequence + index);
            }
        }
        if (m_follows_latest) {
            ImGui::SetScrollHereY(1.0F);
        }

        m_scroll_y = ImGui::GetScrollY();
        m_row_height = clipper.ItemsHeight;
        if (m_row_height > 0.0F) {
            auto const top_row{
                std::min(static_cast<std::size_t>(m_scroll_y / m_row_height), changes.size() - 1)
            };
            m_top_sequence = first_sequence + top_row;
            m_top_offset = m_scroll_y - static_cast<float>(top_row) * m_row_height;
        }
        ImGui::EndTable();
    }

    auto ChangeTable::ScrollToLatest() -> void {
        m_scroll_to_latest = true;
    }

    auto ChangeTable::RowCount() const -> std::size_t {
        return m_session.Changes().size();
    }

    auto ChangeTable::FollowsLatest() const -> bool {
        return m_follows_latest;
    }

    auto ChangeTable::NewChangeCount() const -> std::size_t {
        if (m_follows_latest) {
            return 0;
        }
        auto const first_unseen{ std::max(m_unseen_sequence, m_session.FirstChangeSequence()) };
        return static_cast<std::size_t>(m_end_sequence - std::min(first_unseen, m_end_sequence));
    }

    auto ChangeTable::renderRow(core::FileChange const& change, std::uint64_t sequence) -> void {
        ImGui::TableNextRow();
        // Shade every other change rather than every other row, so a row keeps
        // its shade when the changes above it are dropped.
        auto const row_color{ sequence % 2 == 0 ? ImGuiCol_TableRowBg : ImGuiCol_TableRowBgAlt };
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(row_color));

        ImGui::TableSetColumnIndex(0);
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

    auto ChangeTable::rowAtOrAfter(std::uint64_t sequence) const -> std::size_t {
        auto const first_sequence{ m_session.FirstChangeSequence() };
        if (sequence <= first_sequence) {
            return 0;
        }
        return std::min(static_cast<std::size_t>(sequence - first_sequence), m_session.Changes().size() - 1);
    }

} // namespace file_monitor::gui
