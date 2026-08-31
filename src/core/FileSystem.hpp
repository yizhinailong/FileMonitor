#pragma once

#include <chrono>
#include <filesystem>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace file_monitor::core {

    enum class FileChangeStatus {
        Added,
        Removed,
        Modified,
        Renamed,
        Moved
    };

    struct FileState {
        std::optional<std::chrono::system_clock::time_point> creation_time;
        std::optional<std::filesystem::file_time_type>       modified_time;
        std::optional<std::uintmax_t>                        size;
        std::string                                          absolute_path;
        bool                                                 is_directory{ false };
    };

    struct FileChange {
        std::string      time;
        FileChangeStatus status;
        std::string      size;
        std::string      absolute_path;
        std::string      previous_absolute_path;
    };

    auto read_file_state(std::filesystem::path const& path) -> FileState;
    auto scan_files(
        std::span<std::filesystem::path const> directories,
        std::span<std::filesystem::path const> excluded_directories,
        std::stop_token                        stop_token
    ) -> std::vector<FileState>;
    auto detect_file_changes(
        std::span<FileState const> previous_files,
        std::span<FileState const> current_files
    ) -> std::vector<FileChange>;
    auto make_file_change(
        FileChangeStatus status,
        FileState const& file,
        std::string      previous_absolute_path = {}
    ) -> FileChange;
    auto file_change_status_text(FileChangeStatus status) -> std::string_view;
    auto path_to_utf8(std::filesystem::path const& path) -> std::string;

} // namespace file_monitor::core
