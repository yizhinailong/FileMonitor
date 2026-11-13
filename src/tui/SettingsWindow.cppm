module;

#include <algorithm>
#include <format>
#include <functional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

export module file_monitor.tui:settings_window;

import file_monitor.core;
import ftxui;
import :directory_list;
import :key_hint_bar;
import :path_completion;
import :widgets;

namespace file_monitor::tui {

    // The settings page shows the monitored and the excluded directories
    // above one input that adds a directory to either list.
    export class SettingsWindow final : public ftxui::ComponentBase {
    public:
        SettingsWindow(core::MonitorSession& session, std::function<void()> on_close);

        auto Open() -> void;

        auto OnRender() -> ftxui::Element override;
        auto OnEvent(ftxui::Event event) -> bool override;
        // The page handles its keys itself and has no focusable children, so
        // the container of MainWindow only passes it keys if it says so.
        [[nodiscard]]
        auto Focusable() const -> bool override;

    private:
        enum class Focus {
            Monitored,
            Excluded,
            Input,
        };

        auto handleEvent(ftxui::Event event) -> bool;
        auto onListEvent(ftxui::Event event) -> bool;
        auto onInputEvent(ftxui::Event event) -> bool;
        auto onClick(ftxui::Event event) -> bool;
        auto renderInputLine() -> ftxui::Element;
        auto renderFooter(int width) -> ftxui::Element;
        auto listHints() -> std::vector<KeyHint>;
        auto inputHints() -> std::vector<KeyHint>;
        auto makeInputField() -> ftxui::Component;
        [[nodiscard]]
        auto targetList() -> DirectoryList&;
        auto focusInput() -> void;
        auto focusList() -> void;
        auto switchList() -> void;
        auto addInputDirectory() -> void;
        auto completeInput() -> void;
        auto removeSelectedDirectory() -> void;
        auto refreshWarnings() -> void;
        [[nodiscard]]
        auto hasUnsavedChanges() const -> bool;
        auto save() -> void;
        // Closes the page, or with unsaved changes first asks to press Esc
        // again.
        auto cancel() -> void;

        core::MonitorSession& m_session;
        std::function<void()> m_on_close;
        core::Configuration m_pending_configuration;
        DirectoryList m_monitored_list;
        DirectoryList m_excluded_list;
        Focus m_focus{ Focus::Monitored };
        // The list that the input adds to.
        Focus m_target{ Focus::Monitored };
        std::string m_input;
        int m_input_cursor{ 0 };
        std::string m_input_error;
        // The directories that the last Tab press could complete the input
        // with.
        std::vector<std::string> m_completions;
        ftxui::Component m_input_field;
        ftxui::Box m_input_box{ .x_min = 0, .x_max = -1, .y_min = 0, .y_max = -1 };
        bool m_confirm_discard{ false };
        bool m_cancel_requested{ false };
        KeyHintBar m_list_hints;
        KeyHintBar m_input_hints;
    };

    namespace {

        constexpr auto GAP{ 2 };

        auto is_click(ftxui::Event event) -> bool {
            return event.is_mouse() &&
                event.mouse().button == ftxui::Mouse::Left &&
                event.mouse().motion == ftxui::Mouse::Pressed;
        }

    } // namespace

    SettingsWindow::SettingsWindow(
        core::MonitorSession& session,
        std::function<void()> on_close
    ) : m_session{ session },
        m_on_close{ std::move(on_close) },
        m_monitored_list{ m_pending_configuration.directories, "监控文件夹" },
        m_excluded_list{ m_pending_configuration.excluded_directories, "不监控文件夹" },
        m_input_field{ makeInputField() },
        m_list_hints{ listHints() },
        m_input_hints{ inputHints() } {}

    auto SettingsWindow::Open() -> void {
        m_pending_configuration = m_session.CurrentConfiguration();
        m_monitored_list.Reset();
        m_excluded_list.Reset();
        m_input.clear();
        m_input_cursor = 0;
        m_input_error.clear();
        m_confirm_discard = false;
        m_target = Focus::Monitored;
        // Without a monitored folder, adding one is the only thing to do.
        m_focus = m_pending_configuration.directories.empty() ? Focus::Input : Focus::Monitored;
        refreshWarnings();
    }

    auto SettingsWindow::OnEvent(ftxui::Event event) -> bool {
        m_cancel_requested = false;
        auto const user_input{ is_user_input(event) };
        auto const handled{ handleEvent(std::move(event)) };
        // Anything but a second Esc withdraws the request to discard changes.
        if (user_input && !m_cancel_requested) {
            m_confirm_discard = false;
        }
        return handled;
    }

    auto SettingsWindow::Focusable() const -> bool {
        return true;
    }

    auto SettingsWindow::handleEvent(ftxui::Event event) -> bool {
        if (event == ftxui::Event::CtrlS) {
            save();
            return true;
        }
        if (is_click(event) && onClick(event)) {
            return true;
        }
        return m_focus == Focus::Input ? onInputEvent(std::move(event)) : onListEvent(std::move(event));
    }

    auto SettingsWindow::onListEvent(ftxui::Event event) -> bool {
        if (event == ftxui::Event::Escape) {
            cancel();
            return true;
        }
        if (event == ftxui::Event::Tab || event == ftxui::Event::TabReverse) {
            switchList();
            return true;
        }
        if (event == ftxui::Event::Character('a') || event == ftxui::Event::Insert) {
            focusInput();
            return true;
        }
        if (event == ftxui::Event::Delete || event == ftxui::Event::Character('d')) {
            removeSelectedDirectory();
            return true;
        }
        if (event.is_mouse()) {
            return m_monitored_list.OnEvent(event) || m_excluded_list.OnEvent(event);
        }
        return targetList().OnEvent(std::move(event));
    }

    auto SettingsWindow::onInputEvent(ftxui::Event event) -> bool {
        if (event == ftxui::Event::Escape) {
            focusList();
            return true;
        }
        if (event == ftxui::Event::Return) {
            addInputDirectory();
            return true;
        }
        if (event == ftxui::Event::Tab) {
            completeInput();
            return true;
        }
        if (event.is_mouse()) {
            return m_monitored_list.OnEvent(event) || m_excluded_list.OnEvent(event);
        }
        return m_input_field->OnEvent(std::move(event));
    }

    auto SettingsWindow::onClick(ftxui::Event event) -> bool {
        auto& hints{ m_focus == Focus::Input ? m_input_hints : m_list_hints };
        if (hints.OnEvent(event)) {
            return true;
        }
        auto const& mouse{ event.mouse() };
        if (m_monitored_list.OnClick(mouse.x, mouse.y)) {
            m_focus = m_target = Focus::Monitored;
            return true;
        }
        if (m_excluded_list.OnClick(mouse.x, mouse.y)) {
            m_focus = m_target = Focus::Excluded;
            return true;
        }
        if (m_input_box.Contain(mouse.x, mouse.y)) {
            focusInput();
            return true;
        }
        return false;
    }

    auto SettingsWindow::OnRender() -> ftxui::Element {
        auto const width{ page_width() };
        auto title{
            hasUnsavedChanges() ?
                ftxui::text("有未保存的修改") | ftxui::color(ftxui::Color::Yellow) :
                ftxui::emptyElement()
        };
        ftxui::Elements rows{
            ftxui::hbox({ ftxui::text("设置") | ftxui::bold, ftxui::filler(), std::move(title) }),
            ftxui::separatorLight(),
        };
        if (has_session_errors(m_session)) {
            rows.push_back(session_error_text(m_session));
            rows.push_back(ftxui::separatorLight());
        }
        rows.push_back(m_monitored_list.Render(m_focus == Focus::Monitored, width) | ftxui::flex);
        rows.push_back(ftxui::separatorLight());
        rows.push_back(m_excluded_list.Render(m_focus == Focus::Excluded, width) | ftxui::flex);
        rows.push_back(ftxui::separatorLight());
        rows.push_back(renderInputLine());
        if (!m_completions.empty()) {
            auto const candidates{
                m_completions | std::views::join_with(std::string_view{ "  " }) | std::ranges::to<std::string>()
            };
            rows.push_back(
                ftxui::hbox({
                    ftxui::text(std::format("{} 个匹配：", m_completions.size())),
                    ftxui::text(candidates),
                }) |
                ftxui::dim
            );
        }
        rows.push_back(error_text(m_input_error));
        rows.push_back(ftxui::separatorLight());
        rows.push_back(renderFooter(width));
        return page(ftxui::vbox(std::move(rows)));
    }

    auto SettingsWindow::renderInputLine() -> ftxui::Element {
        auto label{ ftxui::text(std::format("添加到{} > ", targetList().Title())) | ftxui::bold };
        auto input{
            m_focus == Focus::Input ?
                m_input_field->Render() :
                ftxui::text("按 a 输入文件夹路径") | ftxui::dim
        };
        return ftxui::hbox({ std::move(label), std::move(input) | ftxui::flex }) |
            ftxui::reflect(m_input_box);
    }

    auto SettingsWindow::renderFooter(int width) -> ftxui::Element {
        std::string_view const status{ m_confirm_discard ? "有未保存的修改，再按 Esc 放弃" : "" };
        auto& hints{ m_focus == Focus::Input ? m_input_hints : m_list_hints };
        auto const hints_width{ std::max(width - ftxui::string_width(status) - GAP, 0) };
        return ftxui::hbox({
            hints.Render(hints_width),
            ftxui::filler(),
            ftxui::text(std::string{ status }) | ftxui::color(ftxui::Color::Yellow),
        });
    }

    auto SettingsWindow::listHints() -> std::vector<KeyHint> {
        auto const has_directories{ [this] { return !targetList().Directories().empty(); } };
        return {
            { .key = "↑↓", .label = "选择", .priority = 3 },
            { .key = "Tab", .label = "切换列表", .action = [this] { switchList(); }, .priority = 2 },
            { .key = "a", .label = "添加", .action = [this] { focusInput(); }, .priority = 5 },
            { .key = "Delete", .label = "删除", .action = [this] { removeSelectedDirectory(); }, .priority = 4, .available = has_directories },
            { .key = "Ctrl+S", .label = "保存", .action = [this] { save(); }, .priority = 5 },
            { .key = "Esc", .label = "取消", .action = [this] { cancel(); }, .priority = 5 },
        };
    }

    auto SettingsWindow::inputHints() -> std::vector<KeyHint> {
        std::vector<KeyHint> hints;
        hints.push_back({ .key = "Enter", .label = "添加", .action = [this] { addInputDirectory(); }, .priority = 5 });
        hints.push_back({ .key = "Tab", .label = "补全", .action = [this] { completeInput(); }, .priority = 3 });
        hints.push_back({ .key = "Esc", .label = "返回列表", .action = [this] { focusList(); }, .priority = 4 });
        hints.push_back({ .key = "Ctrl+S", .label = "保存", .action = [this] { save(); }, .priority = 5 });
        return hints;
    }

    auto SettingsWindow::makeInputField() -> ftxui::Component {
        ftxui::InputOption option;
        option.placeholder = "输入文件夹路径，相对路径从当前工作目录算起";
        option.multiline = false;
        option.cursor_position = &m_input_cursor;
        option.on_change = [this] {
            m_input_error.clear();
            m_completions.clear();
        };
        option.transform = [](ftxui::InputState state) {
            if (state.is_placeholder) {
                state.element |= ftxui::dim;
            }
            return state.element;
        };
        // Like the filter input of the main view, the input stays outside the
        // component tree and receives the keys while the input is focused.
        return ftxui::Input(&m_input, std::move(option));
    }

    auto SettingsWindow::targetList() -> DirectoryList& {
        return m_target == Focus::Excluded ? m_excluded_list : m_monitored_list;
    }

    auto SettingsWindow::focusInput() -> void {
        if (m_focus != Focus::Input) {
            m_target = m_focus;
            m_focus = Focus::Input;
        }
    }

    auto SettingsWindow::focusList() -> void {
        m_focus = m_target;
        m_input.clear();
        m_input_cursor = 0;
        m_input_error.clear();
        m_completions.clear();
    }

    auto SettingsWindow::switchList() -> void {
        m_target = m_target == Focus::Monitored ? Focus::Excluded : Focus::Monitored;
        if (m_focus != Focus::Input) {
            m_focus = m_target;
        }
    }

    auto SettingsWindow::addInputDirectory() -> void {
        auto& directories{
            m_target == Focus::Excluded ?
                m_pending_configuration.excluded_directories :
                m_pending_configuration.directories
        };
        auto const result{ core::add_directory_input(directories, m_input) };
        if (!result) {
            m_input_error = result.error();
            return;
        }

        m_input.clear();
        m_input_cursor = 0;
        m_input_error.clear();
        m_completions.clear();
        targetList().SelectLast();
        refreshWarnings();
    }

    auto SettingsWindow::completeInput() -> void {
        auto completion{ complete_directory_path(m_input) };
        if (!completion.matched) {
            m_input_error = "没有以此开头的文件夹";
            m_completions.clear();
            return;
        }
        // Assigning the text directly does not run on_change, which would
        // clear the candidates again.
        m_input = std::move(completion.text);
        m_input_cursor = static_cast<int>(m_input.size());
        m_input_error.clear();
        m_completions = std::move(completion.candidates);
    }

    auto SettingsWindow::removeSelectedDirectory() -> void {
        targetList().RemoveSelected();
        refreshWarnings();
    }

    auto SettingsWindow::refreshWarnings() -> void {
        m_monitored_list.SetWarnings(core::monitored_directory_warnings(m_pending_configuration));
        m_excluded_list.SetWarnings(core::excluded_directory_warnings(m_pending_configuration));
    }

    auto SettingsWindow::hasUnsavedChanges() const -> bool {
        auto const& current{ m_session.CurrentConfiguration() };
        return m_pending_configuration.directories != current.directories ||
            m_pending_configuration.excluded_directories != current.excluded_directories;
    }

    auto SettingsWindow::save() -> void {
        if (m_session.SaveConfiguration(m_pending_configuration)) {
            m_on_close();
        }
    }

    auto SettingsWindow::cancel() -> void {
        if (hasUnsavedChanges() && !m_confirm_discard) {
            m_confirm_discard = true;
            m_cancel_requested = true;
            return;
        }
        m_on_close();
    }

} // namespace file_monitor::tui
