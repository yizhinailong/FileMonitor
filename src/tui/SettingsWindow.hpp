#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include "DirectoryEditor.hpp"
#include "core/Configuration.hpp"
#include "core/MonitorSession.hpp"

namespace file_monitor::tui {

    class SettingsWindow final : public ftxui::ComponentBase {
    public:
        SettingsWindow(core::MonitorSession& session, std::function<void()> on_close);

        auto Open() -> void;

        auto OnRender() -> ftxui::Element override;
        auto OnEvent(ftxui::Event event) -> bool override;

    private:
        auto save() -> void;
        auto currentEditor() const -> std::shared_ptr<DirectoryEditor> const&;

        core::MonitorSession& m_session;
        std::function<void()> m_on_close;
        core::Configuration m_pending_configuration;
        std::vector<std::string> m_tab_labels{ "监控文件夹", "不监控文件夹" };
        int m_tab_index{ 0 };

        std::shared_ptr<DirectoryEditor> m_monitored_editor;
        std::shared_ptr<DirectoryEditor> m_excluded_editor;
        ftxui::Component m_tab_toggle;
        ftxui::Component m_editor_tabs;
        ftxui::Component m_cancel_button;
        ftxui::Component m_save_button;
    };

} // namespace file_monitor::tui
