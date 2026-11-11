#pragma once

#include <functional>
#include <memory>
#include <vector>

#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include "ChangeList.hpp"
#include "KeyHintBar.hpp"
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
        [[nodiscard]]
        auto renderHeader(int width) const -> ftxui::Element;
        auto renderFooter(int width) -> ftxui::Element;
        auto keyHints() -> std::vector<KeyHint>;
        auto openSettings() -> void;
        auto closeSettings() -> void;

        core::MonitorSession& m_session;
        std::function<void()> m_on_quit;
        ChangeList m_change_list;
        KeyHintBar m_key_hints;
        std::shared_ptr<SettingsWindow> m_settings_window;
        int m_page{ 0 };
    };

} // namespace file_monitor::tui
