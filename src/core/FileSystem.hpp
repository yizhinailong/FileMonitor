#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace file_monitor::core {

    enum class FileChangeStatus {
        Added,
        Removed,
        Modified
    };

    struct FileState {
        std::optional<std::filesystem::file_time_type> modified_time;
        std::optional<std::uintmax_t>                  size;
        std::string                                    absolute_path;
    };

    struct FileChange {
        std::string      time;
        FileChangeStatus status;
        std::string      size;
        std::string      absolute_path;
    };

    auto scan_files(std::span<std::filesystem::path const> directories) -> std::vector<FileState>;
    auto detect_file_changes(
        std::span<FileState const> previous_files,
        std::span<FileState const> current_files
    ) -> std::vector<FileChange>;
    auto file_change_status_text(FileChangeStatus status) -> std::string_view;
    auto path_to_utf8(std::filesystem::path const& path) -> std::string;

} // namespace file_monitor::core
