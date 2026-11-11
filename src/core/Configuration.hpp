#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace file_monitor::core {

    struct Configuration {
        std::vector<std::filesystem::path> directories;
        std::vector<std::filesystem::path> excluded_directories;
    };

    auto load_configuration(std::filesystem::path const& config_path)
        -> std::expected<Configuration, std::string>;
    auto save_configuration(
        std::filesystem::path const& config_path,
        Configuration const& configuration
    ) -> std::expected<void, std::string>;

    auto add_directory(
        std::vector<std::filesystem::path>& directories,
        std::filesystem::path directory
    ) -> bool;
    auto add_directory_input(
        std::vector<std::filesystem::path>& directories,
        std::string_view input
    ) -> std::expected<void, std::string>;

    // Why a directory of the configuration has no effect, one entry per
    // directory; an empty entry means the directory is fine. A monitored
    // directory has none if it cannot be read as a directory or lies in another
    // monitored directory, an excluded directory if it lies in no monitored
    // directory. Both look at the file system, so call them when the
    // configuration changes rather than for every frame.
    auto monitored_directory_warnings(Configuration const& configuration) -> std::vector<std::string>;
    auto excluded_directory_warnings(Configuration const& configuration) -> std::vector<std::string>;

} // namespace file_monitor::core
