#pragma once

#include <functional>
#include <memory>

#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include "FileChangeTable.hpp"
#include "SettingsWindow.hpp"
#include "core/MonitorSession.hpp"

namespace file_monitor::tui {

    class MainWindow final : public ftxui::ComponentBase {
    public:
        MainWindow(core::MonitorSession& session, std::function<void()> on_quit);

        [[nodiscard]]
        auto Update() -> bool;

        auto OnEvent(ftxui::Event event) -> bool override;

    private:
        auto renderMainView() -> ftxui::Element;
        auto openSettings() -> void;
        auto closeSettings() -> void;

        core::MonitorSession& m_session;
        std::function<void()> m_on_quit;
        FileChangeTable m_change_table;
        std::shared_ptr<SettingsWindow> m_settings_window;
        ftxui::Component m_settings_button;
        bool m_settings_open{ false };
    };

} // namespace file_monitor::tui
