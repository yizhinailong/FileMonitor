#pragma once

#include <array>
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

    // The statuses in the order in which the interfaces list them.
    inline constexpr std::array FILE_CHANGE_STATUSES{
        FileChangeStatus::Added,
        FileChangeStatus::Removed,
        FileChangeStatus::Modified,
        FileChangeStatus::Renamed,
        FileChangeStatus::Moved,
    };
    inline constexpr auto FILE_CHANGE_STATUS_COUNT{ FILE_CHANGE_STATUSES.size() };

    struct FileState {
        std::optional<std::chrono::system_clock::time_point> creation_time;
        std::optional<std::filesystem::file_time_type> modified_time;
        std::optional<std::uintmax_t> size;
        std::string absolute_path;
        bool is_directory{ false };
    };

    struct FileChange {
        std::string time;
        FileChangeStatus status;
        std::string size;
        std::string absolute_path;
        std::string previous_absolute_path;
    };

    auto read_file_state(std::filesystem::path const& path) -> FileState;
    auto scan_files(
        std::span<std::filesystem::path const> directories,
        std::span<std::filesystem::path const> excluded_directories,
        std::stop_token stop_token
    ) -> std::vector<FileState>;
    auto detect_file_changes(
        std::span<FileState const> previous_files,
        std::span<FileState const> current_files
    ) -> std::vector<FileChange>;
    auto make_file_change(
        FileChangeStatus status,
        FileState const& file,
        std::string previous_absolute_path = {}
    ) -> FileChange;
    auto file_change_status_text(FileChangeStatus status) -> std::string_view;
    // A removed file's time reads "created -> removed"; this returns the time of
    // the change itself.
    auto file_change_event_time(FileChange const& change) -> std::string_view;
    // The creation time of a removed file, or an empty string when the change
    // has none.
    auto file_change_creation_time(FileChange const& change) -> std::string_view;

} // namespace file_monitor::core
