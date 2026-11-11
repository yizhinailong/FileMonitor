#include "MonitorSession.hpp"

#include <cstddef>
#include <ranges>
#include <utility>

#include "Utils.hpp"

namespace file_monitor::core {
    namespace {

        constexpr auto MAX_FILE_CHANGES{ std::size_t{ 1000 } };
        constexpr auto CONFIG_FILE_NAME{ "config.json" };

    } // namespace

    MonitorSession::MonitorSession()
        : m_data_directory{ utils::data_directory() },
          m_change_logger{ m_data_directory } {
        auto configuration{ load_configuration(m_data_directory / CONFIG_FILE_NAME) };
        if (configuration) {
            m_configuration = std::move(*configuration);
        } else {
            m_configuration_error = std::move(configuration.error());
        }
        restartMonitor();
    }

    auto MonitorSession::Update() -> MonitorUpdate {
        MonitorUpdate update;

        auto changes{ m_monitor.TakeChanges() };
        if (!changes.empty()) {
            auto const log_result{ m_change_logger.Write(changes) };
            auto log_error{ log_result ? std::string{} : log_result.error() };
            update.errors_changed = log_error != m_log_error;
            m_log_error = std::move(log_error);

            m_changes.append_range(changes | std::views::as_rvalue);
            if (m_changes.size() > MAX_FILE_CHANGES) {
                auto const excess_count{ m_changes.size() - MAX_FILE_CHANGES };
                m_changes.erase(
                    m_changes.begin(),
                    m_changes.begin() + static_cast<std::ptrdiff_t>(excess_count)
                );
                m_first_change_sequence += excess_count;
            }
            update.changes_added = true;
        }

        auto monitor_error{ m_monitor.TakeError() };
        if (!monitor_error.empty()) {
            m_monitor_error = std::move(monitor_error);
            update.errors_changed = true;
        }
        return update;
    }

    auto MonitorSession::SaveConfiguration(Configuration configuration) -> bool {
        auto const save_result{
            save_configuration(m_data_directory / CONFIG_FILE_NAME, configuration)
        };
        if (!save_result) {
            m_configuration_error = save_result.error();
            return false;
        }

        m_configuration_error.clear();
        m_configuration = std::move(configuration);
        restartMonitor();
        return true;
    }

    auto MonitorSession::CurrentConfiguration() const -> Configuration const& {
        return m_configuration;
    }

    auto MonitorSession::Changes() const -> std::span<FileChange const> {
        return m_changes;
    }

    auto MonitorSession::FirstChangeSequence() const -> std::uint64_t {
        return m_first_change_sequence;
    }

    auto MonitorSession::ConfigurationError() const -> std::string const& {
        return m_configuration_error;
    }

    auto MonitorSession::MonitorError() const -> std::string const& {
        return m_monitor_error;
    }

    auto MonitorSession::LogError() const -> std::string const& {
        return m_log_error;
    }

    auto MonitorSession::restartMonitor() -> void {
        m_first_change_sequence += m_changes.size();
        m_changes.clear();
        m_monitor_error.clear();

        // Writing the change log must not produce change events of its own.
        auto excluded_directories{ m_configuration.excluded_directories };
        excluded_directories.emplace_back(m_data_directory);
        m_monitor.Start(m_configuration.directories, excluded_directories);
    }

} // namespace file_monitor::core
