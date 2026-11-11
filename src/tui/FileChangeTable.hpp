#pragma once

#include <cstddef>
#include <string_view>

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>

#include "core/MonitorSession.hpp"

namespace file_monitor::tui {

    class FileChangeTable final {
    public:
        explicit FileChangeTable(core::MonitorSession const& session);

        auto Render() -> ftxui::Element;
        auto OnEvent(ftxui::Event event) -> bool;

        [[nodiscard]]
        auto FollowsLatest() const -> bool;
        auto FollowLatest() -> void;

    private:
        [[nodiscard]]
        auto visibleRowCount() const -> std::size_t;
        [[nodiscard]]
        auto lastFirstRow() const -> std::size_t;
        auto scrollRows(std::ptrdiff_t row_count) -> void;
        auto scrollColumns(int column_count) -> void;
        [[nodiscard]]
        auto emptyText() const -> std::string_view;

        core::MonitorSession const& m_session;
        ftxui::Box m_body_box{ .x_min = 0, .x_max = -1, .y_min = 0, .y_max = -1 };
        std::size_t m_first_row{ 0 };
        int m_first_column{ 0 };
        bool m_follow_latest{ true };
    };

} // namespace file_monitor::tui
