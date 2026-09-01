#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace file_monitor::core::utils {

    auto path_to_utf8(std::filesystem::path const& path) -> std::string;
    auto utf8_to_path(std::string_view path) -> std::filesystem::path;

} // namespace file_monitor::core::utils
