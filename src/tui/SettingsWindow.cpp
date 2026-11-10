#include "SettingsWindow.hpp"

#include <algorithm>
#include <utility>

#include <ftxui/component/component.hpp>
#include <ftxui/screen/terminal.hpp>

#include "Widgets.hpp"

namespace file_monitor::tui {
    namespace {

        constexpr auto MAX_WINDOW_WIDTH{ 100 };
        constexpr auto MAX_WINDOW_HEIGHT{ 30 };

    } // namespace

    SettingsWindow::SettingsWindow(
        core::MonitorSession& session,
        std::function<void()> on_close
    ) : m_session{ session }, m_on_close{ std::move(on_close) } {
        m_monitored_editor = ftxui::Make<DirectoryEditor>(
            m_pending_configuration.directories,
            "输入监控文件夹路径",
            "尚未添加监控文件夹"
        );
        m_excluded_editor = ftxui::Make<DirectoryEditor>(
            m_pending_configuration.excluded_directories,
            "输入不监控的文件夹路径",
            "尚未添加不监控文件夹"
        );
        m_tab_toggle = ftxui::Toggle(&m_tab_labels, &m_tab_index);
        m_editor_tabs = ftxui::Container::Tab(
            { m_monitored_editor, m_excluded_editor },
            &m_tab_index
        );
        m_cancel_button = make_button("取消", [this] { m_on_close(); });
        m_save_button = make_button("保存", [this] { save(); });

        Add(ftxui::Container::Vertical({
            m_tab_toggle,
            m_editor_tabs,
            ftxui::Container::Horizontal({ m_cancel_button, m_save_button }),
        }));
    }

    auto SettingsWindow::Open() -> void {
        m_pending_configuration = m_session.CurrentConfiguration();
        m_monitored_editor->Reset();
        m_excluded_editor->Reset();
        currentEditor()->FocusInput();
    }

    auto SettingsWindow::OnRender() -> ftxui::Element {
        auto const terminal_size{ ftxui::Terminal::Size() };
        auto const width{ std::clamp(terminal_size.dimx - 4, 1, MAX_WINDOW_WIDTH) };
        auto const height{ std::clamp(terminal_size.dimy - 2, 1, MAX_WINDOW_HEIGHT) };

        auto content{
            ftxui::vbox({
                error_text("监控失败", m_session.MonitorError()),
                error_text("配置失败", m_session.ConfigurationError()),
                error_text("日志失败", m_session.LogError()),
                m_tab_toggle->Render(),
                ftxui::separatorLight(),
                m_editor_tabs->Render() | ftxui::flex,
                ftxui::hbox({
                    m_cancel_button->Render() | ftxui::flex,
                    m_save_button->Render() | ftxui::flex,
                }),
            })
        };
        return ftxui::window(ftxui::text(" 设置 "), std::move(content)) |
            ftxui::size(ftxui::WIDTH, ftxui::EQUAL, width) |
            ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, height);
    }

    auto SettingsWindow::OnEvent(ftxui::Event event) -> bool {
        if (event == ftxui::Event::Escape) {
            m_on_close();
            return true;
        }
        return ComponentBase::OnEvent(std::move(event));
    }

    auto SettingsWindow::save() -> void {
        if (m_session.SaveConfiguration(m_pending_configuration)) {
            m_on_close();
        }
    }

    auto SettingsWindow::currentEditor() const -> std::shared_ptr<DirectoryEditor> const& {
        return m_tab_index == 0 ? m_monitored_editor : m_excluded_editor;
    }

} // namespace file_monitor::tui
