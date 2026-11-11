#include "MainWindow.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/mouse.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/string.hpp>

#include "TextLayout.hpp"
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
        constexpr auto FILTER_TEXT_MAX_WIDTH{ 20 };
        constexpr ftxui::Box HIDDEN_BOX{ .x_min = 0, .x_max = -1, .y_min = 0, .y_max = -1 };

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

        auto status_index(core::FileChangeStatus status) -> std::size_t {
            return static_cast<std::size_t>(std::to_underlying(status));
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

    } // namespace

    MainWindow::MainWindow(core::MonitorSession& session, std::function<void()> on_quit)
        : m_session{ session },
          m_on_quit{ std::move(on_quit) },
          m_change_list{ session },
          m_key_hints{ keyHints() },
          m_filter_hints{ filterHints() },
          m_filter_input{ makeFilterInput() } {
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
            return m_editing_filter ? onFilterEvent(std::move(event)) : onMainEvent(std::move(event));
        }
        return ComponentBase::OnEvent(std::move(event));
    }

    auto MainWindow::onMainEvent(ftxui::Event event) -> bool {
        if (event == ftxui::Event::Character('q')) {
            m_on_quit();
            return true;
        }
        if (event == ftxui::Event::Character('s')) {
            openSettings();
            return true;
        }
        if (event == ftxui::Event::Character('/')) {
            m_editing_filter = true;
            return true;
        }
        if (event == ftxui::Event::Character('0') ||
            (event == ftxui::Event::Escape && m_change_list.IsFiltered())) {
            clearFilters();
            return true;
        }
        for (auto const [index, status] : std::views::enumerate(CHANGE_STATUSES)) {
            if (event == ftxui::Event::Character(static_cast<char>('1' + index))) {
                m_change_list.ToggleStatus(status);
                return true;
            }
        }
        return onStatusClick(event) || m_key_hints.OnEvent(event) || m_change_list.OnEvent(event);
    }

    auto MainWindow::onFilterEvent(ftxui::Event event) -> bool {
        if (event == ftxui::Event::Escape) {
            cancelFilterEdit();
            return true;
        }
        if (event == ftxui::Event::Return) {
            m_editing_filter = false;
            return true;
        }
        if (event.is_mouse()) {
            return onStatusClick(event) || m_filter_hints.OnEvent(event) || m_change_list.OnEvent(event);
        }
        // Keys the input does not use, such as ↑ and ↓, still move the
        // selection.
        return m_filter_input->OnEvent(event) || m_change_list.OnEvent(event);
    }

    auto MainWindow::onStatusClick(ftxui::Event event) -> bool {
        if (!event.is_mouse()) {
            return false;
        }
        auto const& mouse{ event.mouse() };
        if (mouse.button != ftxui::Mouse::Left || mouse.motion != ftxui::Mouse::Pressed) {
            return false;
        }
        for (auto const [status, box] : std::views::zip(CHANGE_STATUSES, m_status_boxes)) {
            if (box.Contain(mouse.x, mouse.y)) {
                m_change_list.ToggleStatus(status);
                return true;
            }
        }
        return false;
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
        rows.push_back(m_editing_filter ? renderFilterFooter(width) : renderFooter(width));
        return page(ftxui::vbox(std::move(rows)));
    }

    auto MainWindow::renderHeader(int width) -> ftxui::Element {
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

        // The number of changes per status. A status hidden by the filter
        // loses its color.
        std::array<std::size_t, STATUS_COUNT> counts{};
        for (auto const& change : m_session.Changes()) {
            ++counts[status_index(change.status)];
        }
        Line count_line;
        for (auto&& [status, box] : std::views::zip(CHANGE_STATUSES, m_status_boxes)) {
            box = HIDDEN_BOX;
            auto const label{ std::string{ core::file_change_status_text(status) } };
            auto const count{ std::format(" {}", counts[status_index(status)]) };
            auto count_element{
                m_change_list.ShowsStatus(status) ?
                    ftxui::hbox({ file_change_status_element(status), ftxui::text(count) }) :
                    ftxui::text(label + count) | ftxui::dim
            };
            count_line.Append(std::move(count_element) | ftxui::reflect(box), label + count);
        }

        // Drop the title before the counts when the line is too narrow.
        for (auto const with_title : { true, false }) {
            auto line{ make_line(with_title) };
            if (line.width + GAP + count_line.width <= width) {
                line.elements.push_back(ftxui::filler());
                line.elements.push_back(ftxui::hbox(std::move(count_line.elements)));
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
        if (m_change_list.IsFiltered()) {
            status += std::format("，共 {}", m_session.Changes().size());
        }

        Line left;
        if (!m_filter_text.empty()) {
            auto const filter{ std::format("过滤：{}", shorten_path(m_filter_text, FILTER_TEXT_MAX_WIDTH)) };
            left.Append(ftxui::text(filter) | ftxui::color(ftxui::Color::Yellow), filter);
            left.width += GAP;
            left.elements.push_back(gap());
        }
        auto const hints_width{ std::max(width - left.width - ftxui::string_width(status) - GAP, 0) };
        left.elements.push_back(m_key_hints.Render(hints_width));
        left.elements.push_back(ftxui::filler());
        left.elements.push_back(ftxui::text(std::move(status)) | ftxui::dim);
        return ftxui::hbox(std::move(left.elements));
    }

    auto MainWindow::renderFilterFooter(int width) -> ftxui::Element {
        auto status{ std::format("匹配 {}/{}", m_change_list.RowCount(), m_session.Changes().size()) };
        auto const hints_width{ std::max(width / 2 - ftxui::string_width(status) - GAP, 0) };
        return ftxui::hbox({
            ftxui::text("过滤 > ") | ftxui::bold,
            m_filter_input->Render() | ftxui::flex,
            gap(),
            m_filter_hints.Render(hints_width),
            gap(),
            ftxui::text(std::move(status)) | ftxui::dim,
        });
    }

    auto MainWindow::keyHints() -> std::vector<KeyHint> {
        auto const is_filtered{ [this] { return m_change_list.IsFiltered(); } };
        return {
            { .key = "↑↓", .label = "选择", .priority = 4 },
            { .key = "/", .label = "过滤", .action = [this] { m_editing_filter = true; }, .priority = 3 },
            { .key = "1-5", .label = "状态", .priority = 1 },
            { .key = "0", .label = "清除过滤", .action = [this] { clearFilters(); }, .priority = 3, .available = is_filtered },
            { .key = "Enter", .label = "详情", .action = [this] { m_change_list.ToggleDetails(); }, .priority = 2 },
            { .key = "End", .label = "最新", .action = [this] { m_change_list.FollowLatest(); }, .priority = 3 },
            { .key = "s", .label = "设置", .action = [this] { openSettings(); }, .priority = 5 },
            { .key = "q", .label = "退出", .action = [this] { m_on_quit(); }, .priority = 5 },
        };
    }

    auto MainWindow::filterHints() -> std::vector<KeyHint> {
        std::vector<KeyHint> hints;
        hints.push_back({ .key = "Enter", .label = "确定", .action = [this] { m_editing_filter = false; }, .priority = 1 });
        hints.push_back({ .key = "Esc", .label = "清除", .action = [this] { cancelFilterEdit(); }, .priority = 2 });
        return hints;
    }

    auto MainWindow::makeFilterInput() -> ftxui::Component {
        ftxui::InputOption option;
        option.placeholder = "输入路径中的文字";
        option.multiline = false;
        option.on_change = [this] {
            m_change_list.SetPathFilter(m_filter_text);
        };
        option.transform = [](ftxui::InputState state) {
            if (state.is_placeholder) {
                state.element |= ftxui::dim;
            }
            return state.element;
        };
        // The input stays outside the component tree: MainWindow passes it the
        // keys while the filter is edited, and a component without a parent
        // counts as focused, so it shows its cursor.
        return ftxui::Input(&m_filter_text, std::move(option));
    }

    auto MainWindow::cancelFilterEdit() -> void {
        m_filter_text.clear();
        m_change_list.SetPathFilter({});
        m_editing_filter = false;
    }

    auto MainWindow::clearFilters() -> void {
        m_filter_text.clear();
        m_change_list.ClearFilters();
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
