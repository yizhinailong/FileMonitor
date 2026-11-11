#include "MainWindow.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <ftxui/component/component.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/string.hpp>

#include "Widgets.hpp"
#include "core/FileSystem.hpp"

#define FILE_MONITOR_STRINGIFY_IMPL(value) #value
#define FILE_MONITOR_STRINGIFY(value)      FILE_MONITOR_STRINGIFY_IMPL(value)

namespace file_monitor::tui {
    namespace {

        constexpr std::string_view APP_TITLE = "文件监控 v" FILE_MONITOR_STRINGIFY(FILE_MONITOR_VERSION);
        constexpr auto MAIN_PAGE{ 0 };
        constexpr auto SETTINGS_PAGE{ 1 };
        constexpr auto GAP{ 2 };
        constexpr std::array CHANGE_STATUSES{
            core::FileChangeStatus::Added,
            core::FileChangeStatus::Removed,
            core::FileChangeStatus::Modified,
            core::FileChangeStatus::Renamed,
            core::FileChangeStatus::Moved,
        };

        struct MonitorState {
            std::string_view text;
            ftxui::Color foreground;
            ftxui::Color background;
        };

        auto monitor_state(core::MonitorSession const& session) -> MonitorState {
            if (session.CurrentConfiguration().directories.empty()) {
                return { "未监控", ftxui::Color::Black, ftxui::Color::Yellow };
            }
            if (!session.MonitorError().empty()) {
                return { "出错", ftxui::Color::White, ftxui::Color::Red };
            }
            return { "监控中", ftxui::Color::Black, ftxui::Color::Green };
        }

        auto gap() -> ftxui::Element {
            return ftxui::text(std::string(GAP, ' '));
        }

        // A line of elements together with its width in columns.
        struct Line {
            ftxui::Elements elements;
            int width{ 0 };

            auto Append(ftxui::Element element, std::string_view text) -> void {
                if (!elements.empty()) {
                    elements.push_back(gap());
                    width += GAP;
                }
                elements.push_back(std::move(element));
                width += ftxui::string_width(text);
            }
        };

        auto status_counts(core::MonitorSession const& session) -> Line {
            std::array<std::size_t, CHANGE_STATUSES.size()> counts{};
            for (auto const& change : session.Changes()) {
                ++counts[static_cast<std::size_t>(std::to_underlying(change.status))];
            }

            Line line;
            for (auto const status : CHANGE_STATUSES) {
                auto const label{ std::string{ core::file_change_status_text(status) } };
                auto const count{
                    std::format(" {}", counts[static_cast<std::size_t>(std::to_underlying(status))])
                };
                line.Append(
                    ftxui::hbox({ file_change_status_element(status), ftxui::text(count) }),
                    label + count
                );
            }
            return line;
        }

    } // namespace

    MainWindow::MainWindow(core::MonitorSession& session, std::function<void()> on_quit)
        : m_session{ session },
          m_on_quit{ std::move(on_quit) },
          m_change_list{ session },
          m_key_hints{ keyHints() } {
        m_settings_window = ftxui::Make<SettingsWindow>(m_session, [this] { closeSettings(); });

        auto main_view{ ftxui::Renderer([this] { return renderMainView(); }) };
        // Settings take the whole screen: a dialog over the change table would
        // leave less room for long paths, and a wide glyph under its left edge
        // would hide the dialog border.
        Add(ftxui::Container::Tab({ std::move(main_view), m_settings_window }, &m_page));
    }

    auto MainWindow::Update() -> bool {
        auto const update{ m_session.Update() };
        return update.changes_added || update.errors_changed;
    }

    auto MainWindow::OnEvent(ftxui::Event event) -> bool {
        if (m_page == MAIN_PAGE) {
            if (event == ftxui::Event::Character('q')) {
                m_on_quit();
                return true;
            }
            if (event == ftxui::Event::Character('s')) {
                openSettings();
                return true;
            }
            if (m_key_hints.OnEvent(event) || m_change_list.OnEvent(event)) {
                return true;
            }
        }
        return ComponentBase::OnEvent(std::move(event));
    }

    auto MainWindow::renderMainView() -> ftxui::Element {
        auto const width{ page_width() };
        ftxui::Elements rows{ renderHeader(width), ftxui::separatorLight() };
        if (has_session_errors(m_session)) {
            rows.push_back(session_error_text(m_session));
            rows.push_back(ftxui::separatorLight());
        }
        rows.push_back(m_change_list.Render(width) | ftxui::flex);
        rows.push_back(m_change_list.RenderDetails(width));
        rows.push_back(ftxui::separatorLight());
        rows.push_back(renderFooter(width));
        return page(ftxui::vbox(std::move(rows)));
    }

    auto MainWindow::renderHeader(int width) const -> ftxui::Element {
        auto const& configuration{ m_session.CurrentConfiguration() };
        auto const state{ monitor_state(m_session) };
        auto const badge{ std::format(" {} ", state.text) };
        auto const summary{
            configuration.excluded_directories.empty() ?
                std::format("{} 个文件夹", configuration.directories.size()) :
                std::format(
                    "{} 个文件夹，排除 {} 个",
                    configuration.directories.size(),
                    configuration.excluded_directories.size()
                )
        };

        auto const make_line{ [&](bool with_title) {
            Line line;
            if (with_title) {
                line.Append(ftxui::text(std::string{ APP_TITLE }) | ftxui::bold, APP_TITLE);
            }
            line.Append(
                ftxui::text(badge) | ftxui::color(state.foreground) | ftxui::bgcolor(state.background),
                badge
            );
            line.Append(ftxui::text(summary), summary);
            return line;
        } };

        // Drop the title before the counts when the line is too narrow.
        auto counts{ status_counts(m_session) };
        for (auto const with_title : { true, false }) {
            auto line{ make_line(with_title) };
            if (line.width + GAP + counts.width <= width) {
                line.elements.push_back(ftxui::filler());
                line.elements.push_back(ftxui::hbox(std::move(counts.elements)));
                return ftxui::hbox(std::move(line.elements));
            }
        }
        return ftxui::hbox(make_line(true).elements);
    }

    auto MainWindow::renderFooter(int width) -> ftxui::Element {
        std::string status;
        if (m_change_list.FollowsLatest()) {
            status = "跟随最新";
        } else if (auto const new_change_count{ m_change_list.NewChangeCount() }; new_change_count > 0) {
            status = std::format("有 {} 条新变更", new_change_count);
        } else {
            status = "已停止跟随";
        }
        if (m_change_list.RowCount() > 0) {
            status += std::format("  {}/{}", m_change_list.SelectedPosition(), m_change_list.RowCount());
        }

        auto const hints_width{ std::max(width - ftxui::string_width(status) - GAP, 0) };
        return ftxui::hbox({
            m_key_hints.Render(hints_width),
            ftxui::filler(),
            ftxui::text(std::move(status)) | ftxui::dim,
        });
    }

    auto MainWindow::keyHints() -> std::vector<KeyHint> {
        return {
            { .key = "↑↓", .label = "选择", .priority = 4 },
            { .key = "Enter", .label = "详情", .action = [this] { m_change_list.ToggleDetails(); }, .priority = 2 },
            { .key = "End", .label = "最新", .action = [this] { m_change_list.FollowLatest(); }, .priority = 3 },
            { .key = "s", .label = "设置", .action = [this] { openSettings(); }, .priority = 5 },
            { .key = "q", .label = "退出", .action = [this] { m_on_quit(); }, .priority = 5 },
        };
    }

    auto MainWindow::openSettings() -> void {
        m_settings_window->Open();
        m_page = SETTINGS_PAGE;
    }

    auto MainWindow::closeSettings() -> void {
        m_page = MAIN_PAGE;
    }

} // namespace file_monitor::tui

#undef FILE_MONITOR_STRINGIFY
#undef FILE_MONITOR_STRINGIFY_IMPL
