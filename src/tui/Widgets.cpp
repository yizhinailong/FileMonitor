#include "Widgets.hpp"

#include <algorithm>
#include <format>
#include <ranges>
#include <utility>

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/screen/terminal.hpp>

namespace file_monitor::tui {
    namespace {

        constexpr auto PAGE_MARGIN{ 1 };

    } // namespace

    auto make_button(
        std::string label,
        std::function<void()> on_click,
        std::function<bool()> enabled
    ) -> ftxui::Component {
        auto is_enabled{ [enabled = std::move(enabled)] {
            return !enabled || enabled();
        } };

        ftxui::ButtonOption option;
        option.label = std::move(label);
        option.on_click = [on_click = std::move(on_click), is_enabled] {
            if (is_enabled()) {
                on_click();
            }
        };
        option.transform = [is_enabled](ftxui::EntryState const& state) {
            auto element{ ftxui::text(" " + state.label + " ") | ftxui::hcenter | ftxui::borderLight };
            if (!is_enabled()) {
                return element | ftxui::dim;
            }
            if (state.focused) {
                element |= ftxui::inverted;
            }
            return element;
        };
        return ftxui::Button(std::move(option));
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

    auto page_width() -> int {
        return std::max(ftxui::Terminal::Size().dimx - 2 * PAGE_MARGIN, 0);
    }

    auto page(ftxui::Element content) -> ftxui::Element {
        auto const margin{ ftxui::text(std::string(PAGE_MARGIN, ' ')) };
        return ftxui::hbox({ margin, std::move(content) | ftxui::flex, margin });
    }

} // namespace file_monitor::tui
