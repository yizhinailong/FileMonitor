#include "FileSystem.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <format>
#include <system_error>

namespace file_monitor::core {
    namespace {

        auto format_file_size(std::filesystem::directory_entry const& entry) -> std::string {
            std::error_code size_error;
            auto const      size{ entry.file_size(size_error) };
            if (size_error) {
                return "Unknown";
            }

            constexpr std::array UNITS{ "B", "KB", "MB", "GB", "TB", "PB" };
            constexpr auto       UNIT_SIZE{ 1024.0 };

            auto       display_size{ static_cast<double>(size) };
            auto       unit_index{ std::size_t{ 0 } };
            auto const last_unit_index{ UNITS.size() - 1 };
            while (display_size >= UNIT_SIZE && unit_index < last_unit_index) {
                display_size /= UNIT_SIZE;
                ++unit_index;
            }

            if (unit_index == 0) {
                return std::format("{} {}", size, UNITS[unit_index]);
            }
            if (display_size >= 100.0) {
                return std::format("{:.0f} {}", display_size, UNITS[unit_index]);
            }
            if (display_size >= 10.0) {
                return std::format("{:.1f} {}", display_size, UNITS[unit_index]);
            }
            return std::format("{:.2f} {}", display_size, UNITS[unit_index]);
        }

        auto format_modified_time(std::filesystem::directory_entry const& entry) -> std::string {
            std::error_code time_error;
            auto const      file_time{ entry.last_write_time(time_error) };
            if (time_error) {
                return "Unknown";
            }

            auto const system_time{ std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                file_time - decltype(file_time)::clock::now() + std::chrono::system_clock::now()
            ) };
            auto const whole_seconds{ std::chrono::floor<std::chrono::seconds>(system_time) };
            auto const milliseconds{
                std::chrono::duration_cast<std::chrono::milliseconds>(system_time - whole_seconds)
                    .count()
            };
            return std::format(
                "{:%Y-%m-%d %H:%M:%S}.{:03}",
                whole_seconds,
                milliseconds
            );
        }

        auto absolute_path_to_utf8(std::filesystem::path const& path) -> std::string {
            std::error_code absolute_error;
            auto const      absolute_path{ std::filesystem::absolute(path, absolute_error) };
            return path_to_utf8(absolute_error ? path : absolute_path);
        }

    } // namespace

    auto load_files(std::filesystem::path const& directory) -> std::vector<FileInfo> {
        std::vector<FileInfo> files;
        std::error_code       iteration_error;
        auto                  iterator{
            std::filesystem::
                recursive_directory_iterator{
                                             directory,
                                             std::filesystem::directory_options::skip_permission_denied,
                                             iteration_error }
        };
        auto const end{ std::filesystem::recursive_directory_iterator{} };

        while (!iteration_error && iterator != end) {
            std::error_code type_error;
            if (iterator->is_regular_file(type_error) && !type_error) {
                files.emplace_back(
                    FileInfo{
                        .modified_time = format_modified_time(*iterator),
                        .size          = format_file_size(*iterator),
                        .absolute_path = absolute_path_to_utf8(iterator->path()) }
                );
            }
            iterator.increment(iteration_error);
        }

        std::ranges::sort(files, {}, &FileInfo::absolute_path);
        return files;
    }

    auto path_to_utf8(std::filesystem::path const& path) -> std::string {
        auto const utf8_path{ path.u8string() };
        return { reinterpret_cast<char const*>(utf8_path.data()), utf8_path.size() };
    }

} // namespace file_monitor::core
