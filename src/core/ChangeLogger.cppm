module;

#include <expected>
#include <filesystem>
#include <format>
#include <memory>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>

export module file_monitor.core:change_logger;

export import :file_system;
import :utils;

namespace file_monitor::core {
    namespace {

        auto change_event_date(std::string_view event_time) -> std::string {
            if (event_time.size() < 10) {
                return "unknown-date";
            }

            auto const date{ event_time.substr(0, 10) };
            for (auto [index, character] : std::views::enumerate(date)) {
                if (index == 4 || index == 7) {
                    if (character != '-') {
                        return "unknown-date";
                    }
                } else if (character < '0' || character > '9') {
                    return "unknown-date";
                }
            }
            return std::string{ date };
        }

        auto change_path_text(FileChange const& change) -> std::string {
            if (change.previous_absolute_path.empty()) {
                return change.absolute_path;
            }
            return std::format(
                "{} -> {}",
                change.previous_absolute_path,
                change.absolute_path
            );
        }

    } // namespace

    class ChangeLogger final {
    public:
        explicit ChangeLogger(std::filesystem::path log_directory)
            : m_log_directory{ std::move(log_directory) } {
        }

        ChangeLogger(ChangeLogger const&) = delete;
        ChangeLogger(ChangeLogger&&) = delete;
        auto operator=(ChangeLogger const&) -> ChangeLogger& = delete;
        auto operator=(ChangeLogger&&) -> ChangeLogger& = delete;

        auto Write(std::span<FileChange const> changes)
            -> std::expected<void, std::string> {
            for (auto const& change : changes) {
                auto const date{ change_event_date(file_change_event_time(change)) };
                auto const logger_result{ ensureLogger(date) };
                if (!logger_result) {
                    return logger_result;
                }

                m_error_message.clear();
                try {
                    m_logger->info(
                        "[{}] [{}] [{}] {}",
                        change.time,
                        file_change_status_text(change.status),
                        change.size,
                        change_path_text(change)
                    );
                } catch (spdlog::spdlog_ex const& exception) {
                    return std::unexpected{
                        std::format("写入日志失败：{}", exception.what())
                    };
                }
                if (!m_error_message.empty()) {
                    return std::unexpected{
                        std::format("写入日志失败：{}", m_error_message)
                    };
                }
            }
            return {};
        }

    private:
        auto ensureLogger(std::string const& date) -> std::expected<void, std::string> {
            if (m_logger && m_current_date == date) {
                return {};
            }

            std::error_code create_error;
            std::filesystem::create_directories(m_log_directory, create_error);
            if (create_error) {
                return std::unexpected{
                    std::format(
                        "创建日志目录失败 {}：{}",
                        utils::path_to_utf8(m_log_directory),
                        create_error.message()
                    )
                };
            }

            auto const log_path{ m_log_directory / (date + ".log") };
            try {
                auto sink{
                    std::make_shared<spdlog::sinks::basic_file_sink_mt>(
                        utils::path_to_utf8(log_path),
                        false
                    )
                };
                auto new_logger{
                    std::make_shared<spdlog::logger>("file-monitor-change-log", sink)
                };
                new_logger->set_pattern("%v");
                new_logger->flush_on(spdlog::level::info);
                new_logger->set_error_handler([this](std::string const& message) {
                    m_error_message = message;
                });

                m_logger = std::move(new_logger);
                m_current_date = date;
            } catch (spdlog::spdlog_ex const& exception) {
                return std::unexpected{
                    std::format(
                        "打开日志文件失败 {}：{}",
                        utils::path_to_utf8(log_path),
                        exception.what()
                    )
                };
            }
            return {};
        }

        std::filesystem::path m_log_directory;
        std::string m_current_date;
        std::string m_error_message;
        std::shared_ptr<spdlog::logger> m_logger;
    };

} // namespace file_monitor::core
