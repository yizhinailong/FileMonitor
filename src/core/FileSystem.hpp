#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace file_monitor::core {

    struct FileInfo {
        std::string modified_time;
        std::string size;
        std::string absolute_path;
    };

    auto load_files(std::filesystem::path const& directory) -> std::vector<FileInfo>;
    auto path_to_utf8(std::filesystem::path const& path) -> std::string;

} // namespace file_monitor::core
