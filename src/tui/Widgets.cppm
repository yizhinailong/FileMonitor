module;

#include <algorithm>
#include <format>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>

export module file_monitor.tui:widgets;

import file_monitor.core;
import ftxui;

namespace file_monitor::tui {

    // Keys and clicks, as opposed to mouse moves and the redraw requests of
    // the session poller.
    export [[nodiscard]] auto is_user_input(ftxui::Event event) -> bool;
    export auto error_text(std::string_view message) -> ftxui::Element;
    export auto error_text(std::string_view label, std::string_view message) -> ftxui::Element;
    export auto session_error_text(core::MonitorSession const& session) -> ftxui::Element;
    export [[nodiscard]] auto has_session_errors(core::MonitorSession const& session) -> bool;
    // The status text in its color.
    export auto file_change_status_element(core::FileChangeStatus status) -> ftxui::Element;

    // A page fills the terminal and keeps one blank column on either side.
    export [[nodiscard]] auto page_width() -> int;
    export auto page(ftxui::Element content) -> ftxui::Element;

    namespace {

        constexpr auto PAGE_MARGIN{ 1 };

        auto file_change_status_color(core::FileChangeStatus status) -> ftxui::Color {
            switch (status) {
                case core::FileChangeStatus::Added:
                    return ftxui::Color::Green;
                case core::FileChangeStatus::Removed:
                    return ftxui::Color::Red;
                case core::FileChangeStatus::Modified:
                    return ftxui::Color::Yellow;
                case core::FileChangeStatus::Renamed:
                    return ftxui::Color::Cyan;
                case core::FileChangeStatus::Moved:
                    return ftxui::Color::Magenta;
            }
            return ftxui::Color::Default;
        }

    } // namespace

    auto is_user_input(ftxui::Event event) -> bool {
        if (event == ftxui::Event::Custom) {
            return false;
        }
        if (event.is_mouse()) {
            return event.mouse().motion == ftxui::Mouse::Pressed;
        }
        return true;
    }

    auto error_text(std::string_view message) -> ftxui::Element {
        if (message.empty()) {
            return ftxui::emptyElement();
        }

        ftxui::Elements lines;
        for (auto const line : message | std::views::split('\n')) {
            lines.push_back(ftxui::paragraph(std::string_view{ line.data(), line.size() }));
        }
        return ftxui::vbox(std::move(lines)) | ftxui::color(ftxui::Color::Red);
    }

    auto error_text(std::string_view label, std::string_view message) -> ftxui::Element {
        if (message.empty()) {
            return ftxui::emptyElement();
        }
        return error_text(std::format("{}：{}", label, message));
    }

    auto session_error_text(core::MonitorSession const& session) -> ftxui::Element {
        return ftxui::vbox({
            error_text("监控失败", session.MonitorError()),
            error_text("配置失败", session.ConfigurationError()),
            error_text("日志失败", session.LogError()),
        });
    }

    auto has_session_errors(core::MonitorSession const& session) -> bool {
        return !session.MonitorError().empty() ||
            !session.ConfigurationError().empty() ||
            !session.LogError().empty();
    }

    auto file_change_status_element(core::FileChangeStatus status) -> ftxui::Element {
        return ftxui::text(std::string{ core::file_change_status_text(status) }) |
            ftxui::color(file_change_status_color(status));
    }

    auto page_width() -> int {
        return std::max(ftxui::Terminal::Size().dimx - 2 * PAGE_MARGIN, 0);
    }

    auto page(ftxui::Element content) -> ftxui::Element {
        auto const margin{ ftxui::text(std::string(PAGE_MARGIN, ' ')) };
        return ftxui::hbox({ margin, std::move(content) | ftxui::flex, margin });
    }

} // namespace file_monitor::tui
