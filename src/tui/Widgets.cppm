export module file_monitor.tui:widgets;

import std;
import file_monitor.core;
import ftxui;

namespace file_monitor::tui {

    namespace {

        constexpr auto PAGE_MARGIN{ 1 };

        ftxui::Color file_change_status_color(core::FileChangeStatus status)
        {
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

    // Keys and clicks, as opposed to mouse moves and the redraw requests of
    // the session poller.
    export [[nodiscard]] bool is_user_input(ftxui::Event event)
    {
        if (event == ftxui::Event::Custom) {
            return false;
        }
        if (event.is_mouse()) {
            return event.mouse().motion == ftxui::Mouse::Pressed;
        }
        return true;
    }

    export ftxui::Element error_text(std::string_view message)
    {
        if (message.empty()) {
            return ftxui::emptyElement();
        }

        ftxui::Elements lines;
        for (auto const line : message | std::views::split('\n')) {
            lines.push_back(ftxui::paragraph(std::string_view{ line.data(), line.size() }));
        }
        return ftxui::vbox(std::move(lines)) | ftxui::color(ftxui::Color::Red);
    }

    export ftxui::Element error_text(std::string_view label, std::string_view message)
    {
        if (message.empty()) {
            return ftxui::emptyElement();
        }
        return error_text(std::format("{}：{}", label, message));
    }

    export ftxui::Element session_error_text(core::MonitorSession const& session)
    {
        return ftxui::vbox({
            error_text("监控失败", session.MonitorError()),
            error_text("配置失败", session.ConfigurationError()),
            error_text("日志失败", session.LogError()),
        });
    }

    export [[nodiscard]] bool has_session_errors(core::MonitorSession const& session)
    {
        return !session.MonitorError().empty() ||
            !session.ConfigurationError().empty() ||
            !session.LogError().empty();
    }

    // The status text in its color.
    export ftxui::Element file_change_status_element(core::FileChangeStatus status)
    {
        return ftxui::text(std::string{ core::file_change_status_text(status) }) |
            ftxui::color(file_change_status_color(status));
    }

    // A page fills the terminal and keeps one blank column on either side.
    export [[nodiscard]] int page_width()
    {
        return std::max(ftxui::Terminal::Size().dimx - 2 * PAGE_MARGIN, 0);
    }

    export ftxui::Element page(ftxui::Element content)
    {
        auto const margin{ ftxui::text(std::string(PAGE_MARGIN, ' ')) };
        return ftxui::hbox({ margin, std::move(content) | ftxui::flex, margin });
    }

} // namespace file_monitor::tui
