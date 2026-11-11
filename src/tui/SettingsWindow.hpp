#pragma once

#include <functional>
#include <string>
#include <vector>

#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>

#include "DirectoryList.hpp"
#include "KeyHintBar.hpp"
#include "core/Configuration.hpp"
#include "core/MonitorSession.hpp"

namespace file_monitor::tui {

    // The settings page shows the monitored and the excluded directories
    // above one input that adds a directory to either list.
    class SettingsWindow final : public ftxui::ComponentBase {
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

} // namespace file_monitor::tui
