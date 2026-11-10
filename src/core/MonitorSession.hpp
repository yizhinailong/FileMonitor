#pragma once

#include <filesystem>
#include <span>
#include <string>
#include <vector>

#include "ChangeLogger.hpp"
#include "Configuration.hpp"
#include "DirectoryMonitor.hpp"
#include "FileSystem.hpp"

namespace file_monitor::core {

    struct MonitorUpdate {
        bool changes_added{ false };
        bool errors_changed{ false };
    };

    class MonitorSession final {
    public:
        MonitorSession();

        auto Update() -> MonitorUpdate;
        [[nodiscard]]
        auto SaveConfiguration(Configuration configuration) -> bool;

        [[nodiscard]]
        auto CurrentConfiguration() const -> Configuration const&;
        [[nodiscard]]
        auto Changes() const -> std::span<FileChange const>;
        [[nodiscard]]
        auto ConfigurationError() const -> std::string const&;
        [[nodiscard]]
        auto MonitorError() const -> std::string const&;
        [[nodiscard]]
        auto LogError() const -> std::string const&;

    private:
        auto restartMonitor() -> void;

        std::filesystem::path m_data_directory;
        Configuration m_configuration;
        std::vector<FileChange> m_changes;
        DirectoryMonitor m_monitor;
        ChangeLogger m_change_logger;
        std::string m_configuration_error;
        std::string m_monitor_error;
        std::string m_log_error;
    };

} // namespace file_monitor::core
