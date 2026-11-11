#pragma once

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>

#include "ChangeList.hpp"
#include "KeyHintBar.hpp"
#include "SettingsWindow.hpp"
#include "core/FileSystem.hpp"
#include "core/MonitorSession.hpp"

namespace file_monitor::tui {

    class MainWindow final : public ftxui::ComponentBase {
    public:
        MainWindow(core::MonitorSession& session, std::function<void()> on_quit);

        [[nodiscard]]
        auto Update() -> bool;

        auto OnEvent(ftxui::Event event) -> bool override;

    private:
        auto onMainEvent(ftxui::Event event) -> bool;
        auto onFilterEvent(ftxui::Event event) -> bool;
        auto onStatusClick(ftxui::Event event) -> bool;
        auto renderMainView() -> ftxui::Element;
        auto renderHeader(int width) -> ftxui::Element;
        auto renderFooter(int width) -> ftxui::Element;
        auto renderFilterFooter(int width) -> ftxui::Element;
        auto keyHints() -> std::vector<KeyHint>;
        auto filterHints() -> std::vector<KeyHint>;
        auto makeFilterInput() -> ftxui::Component;
        // Clears the path filter and stops editing it.
        auto cancelFilterEdit() -> void;
        auto clearFilters() -> void;
        auto copySelectedPath() -> void;
        auto showSelectedChange() -> void;
        auto openSettings() -> void;
        auto closeSettings() -> void;

        core::MonitorSession& m_session;
        std::function<void()> m_on_quit;
        ChangeList m_change_list;
        KeyHintBar m_key_hints;
        KeyHintBar m_filter_hints;
        std::string m_filter_text;
        ftxui::Component m_filter_input;
        bool m_editing_filter{ false };
        // The outcome of copying or showing a path, shown in the footer until
        // the next key or click.
        std::string m_notice;
        bool m_notice_failed{ false };
        // The status counts of the header, which toggle the status filters.
        std::array<ftxui::Box, core::FILE_CHANGE_STATUS_COUNT> m_status_boxes{};
        std::shared_ptr<SettingsWindow> m_settings_window;
        int m_page{ 0 };
    };

} // namespace file_monitor::tui
