#pragma once

#include <cstdint>

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
        // The change table as it was in the previous frame, used to keep its
        // rows in place when the session drops the oldest changes.
        std::uint64_t m_first_change_sequence{ 0 };
        float m_table_scroll_y{ 0.0F };
        float m_table_row_height{ 0.0F };
        bool m_table_following_latest{ true };
    };

} // namespace file_monitor::gui
