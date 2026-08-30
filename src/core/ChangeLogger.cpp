#include "ChangeLogger.hpp"

#include <format>
#include <string_view>
#include <system_error>
#include <utility>

#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>

namespace file_monitor::core {
    namespace {

        auto change_event_date(std::string_view time) -> std::string {
            auto date_offset{ std::size_t{ 0 } };
            auto const separator{ time.rfind(" -> ") };
            if (separator != std::string_view::npos) {
                date_offset = separator + 4;
            }
            if (time.size() < date_offset + 10) {
                return "unknown-date";
            }

            auto const date{ time.substr(date_offset, 10) };
            for (auto index{ std::size_t{ 0 } }; index < date.size(); ++index) {
                if (index == 4 || index == 7) {
                    if (date[index] != '-') {
                        return "unknown-date";
                    }
                } else if (date[index] < '0' || date[index] > '9') {
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

    struct ChangeLogger::Impl {
        explicit Impl(std::filesystem::path directory)
            : log_directory{ std::move(directory) } {
        }

        auto ensureLogger(std::string const& date) -> std::expected<void, std::string> {
            if (logger && current_date == date) {
                return {};
            }

            std::error_code create_error;
            std::filesystem::create_directories(log_directory, create_error);
            if (create_error) {
                return std::unexpected{
                    std::format(
                        "创建日志目录失败 {}：{}",
                        path_to_utf8(log_directory),
                        create_error.message()
                    )
                };
            }

            auto const log_path{ log_directory / (date + ".log") };
            try {
                auto sink{
                    std::make_shared<spdlog::sinks::basic_file_sink_mt>(
                        path_to_utf8(log_path),
                        false
                    )
                };
                auto new_logger{
                    std::make_shared<spdlog::logger>("file-monitor-change-log", sink)
                };
                new_logger->set_pattern("%v");
                new_logger->flush_on(spdlog::level::info);
                new_logger->set_error_handler([this](std::string const& message) {
                    error_message = message;
                });

                logger       = std::move(new_logger);
                current_date = date;
            } catch (spdlog::spdlog_ex const& exception) {
                return std::unexpected{
                    std::format(
                        "打开日志文件失败 {}：{}",
                        path_to_utf8(log_path),
                        exception.what()
                    )
                };
            }
            return {};
        }

        std::filesystem::path          log_directory;
        std::string                    current_date;
        std::string                    error_message;
        std::shared_ptr<spdlog::logger> logger;
    };

    ChangeLogger::ChangeLogger(std::filesystem::path log_directory)
        : m_impl{ std::make_unique<Impl>(std::move(log_directory)) } {
    }

    ChangeLogger::~ChangeLogger() = default;

    auto ChangeLogger::Write(
        std::span<FileChange const> changes,
        std::uint64_t               first_record_number
    )
        -> std::expected<void, std::string> {
        auto record_number{ first_record_number };
        for (auto const& change : changes) {
            auto const date{ change_event_date(change.time) };
            auto const logger_result{ m_impl->ensureLogger(date) };
            if (!logger_result) {
                return logger_result;
            }

            m_impl->error_message.clear();
            try {
                m_impl->logger->info(
                    "{} | {} | {} | {} | {}",
                    record_number,
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
            if (!m_impl->error_message.empty()) {
                return std::unexpected{
                    std::format("写入日志失败：{}", m_impl->error_message)
                };
            }
            ++record_number;
        }
        return {};
    }

} // namespace file_monitor::core
