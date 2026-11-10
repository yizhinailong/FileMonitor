#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace file_monitor::core::utils {

    auto path_to_utf8(std::filesystem::path const& path) -> std::string;
    auto utf8_to_path(std::string_view path) -> std::filesystem::path;

    auto executable_directory() -> std::filesystem::path;
    auto path_is_within(
        std::filesystem::path const& path,
        std::filesystem::path const& directory
    ) -> bool;
    auto paths_equal(
        std::filesystem::path const& left,
        std::filesystem::path const& right
    ) -> bool;

} // namespace file_monitor::core::utils
