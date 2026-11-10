#pragma once

#include "SettingsWindow.hpp"
#include "core/FileSystem.hpp"
#include "core/MonitorSession.hpp"

namespace file_monitor::gui {
    class MainWindow final {
    public:
        [[nodiscard]]
        auto Settings() -> SettingsWindow&;

        auto Render() -> void;

    private:
        auto renderSettingsPanel(float width, float height) -> void;
        auto renderErrorPanel(float width) -> void;
        auto renderFileListPanel(float width, float height) -> void;
        auto renderFileChangeRow(core::FileChange const& change) -> void;
        [[nodiscard]]
        auto calculatePathColumnWidth() const -> float;

        core::MonitorSession m_session;
        SettingsWindow m_settings_window;
        bool m_scroll_to_latest{ false };
    };

} // namespace file_monitor::gui
