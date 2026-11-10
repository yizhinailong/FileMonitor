#include "MainWindow.hpp"

#include <format>
#include <string_view>
#include <utility>

#include <ftxui/component/component.hpp>

#include "Widgets.hpp"

#define FILE_MONITOR_STRINGIFY_IMPL(value) #value
#define FILE_MONITOR_STRINGIFY(value)      FILE_MONITOR_STRINGIFY_IMPL(value)

namespace file_monitor::tui {
    namespace {

        constexpr std::string_view WINDOW_TITLE = " 文件监控 v" FILE_MONITOR_STRINGIFY(FILE_MONITOR_VERSION) " ";
        constexpr std::string_view KEY_HINTS =
            "s 设置  q 退出  ↑↓ PgUp PgDn Home End 滚动  ←→ 横向滚动";

    } // namespace

    MainWindow::MainWindow(core::MonitorSession& session, std::function<void()> on_quit)
        : m_session{ session }, m_on_quit{ std::move(on_quit) }, m_change_table{ session } {
        m_settings_window = ftxui::Make<SettingsWindow>(m_session, [this] { closeSettings(); });
        m_settings_button = make_button("设置", [this] { openSettings(); });

        auto main_view{
            ftxui::Renderer(m_settings_button, [this] { return renderMainView(); })
        };
        Add(ftxui::Modal(std::move(main_view), m_settings_window, &m_settings_open));
    }

    auto MainWindow::Update() -> bool {
        auto const update{ m_session.Update() };
        if (update.changes_added) {
            m_change_table.ScrollToLatest();
        }
        return update.changes_added || update.errors_changed;
    }

    auto MainWindow::OnEvent(ftxui::Event event) -> bool {
        if (!m_settings_open) {
            if (event == ftxui::Event::Character('q')) {
                m_on_quit();
                return true;
            }
            if (event == ftxui::Event::Character('s')) {
                openSettings();
                return true;
            }
            if (m_change_table.OnEvent(event)) {
                return true;
            }
        }
        return ComponentBase::OnEvent(std::move(event));
    }

    auto MainWindow::renderMainView() -> ftxui::Element {
        auto const& configuration{ m_session.CurrentConfiguration() };
        auto const summary{
            std::format(
                "监控 {} 个文件夹，排除 {} 个文件夹",
                configuration.directories.size(),
                configuration.excluded_directories.size()
            )
        };
        auto content{
            ftxui::vbox({
                ftxui::hbox({
                    m_settings_button->Render(),
                    ftxui::text(summary) | ftxui::vcenter,
                }),
                m_change_table.Render() | ftxui::flex,
                ftxui::separatorLight(),
                ftxui::text(KEY_HINTS) | ftxui::dim,
            })
        };
        return ftxui::window(ftxui::text(WINDOW_TITLE), std::move(content));
    }

    auto MainWindow::openSettings() -> void {
        m_settings_window->Open();
        m_settings_open = true;
    }

    auto MainWindow::closeSettings() -> void {
        m_settings_open = false;
        m_settings_button->TakeFocus();
    }

} // namespace file_monitor::tui

#undef FILE_MONITOR_STRINGIFY
#undef FILE_MONITOR_STRINGIFY_IMPL
