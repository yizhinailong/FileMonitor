#include "FileSystem.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <format>
#include <optional>
#include <system_error>

#if defined(_WIN32)
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <Windows.h>
#endif

namespace file_monitor::core {
    namespace {

        using SystemTime = std::chrono::system_clock::time_point;

        auto read_creation_time(std::filesystem::path const& path)
            -> std::optional<SystemTime> {
#if defined(_WIN32)
            WIN32_FILE_ATTRIBUTE_DATA attributes{};
            if (!GetFileAttributesExW(
                    path.c_str(),
                    GetFileExInfoStandard,
                    &attributes
                )) {
                return std::nullopt;
            }

            ULARGE_INTEGER windows_time{};
            windows_time.LowPart  = attributes.ftCreationTime.dwLowDateTime;
            windows_time.HighPart = attributes.ftCreationTime.dwHighDateTime;

            constexpr auto WINDOWS_EPOCH_OFFSET_TICKS{
                std::int64_t{ 116'444'736'000'000'000 }
            };
            using WindowsDuration = std::chrono::duration<std::int64_t, std::ratio<1, 10'000'000>>;
            auto const unix_ticks{
                static_cast<std::int64_t>(windows_time.QuadPart) - WINDOWS_EPOCH_OFFSET_TICKS
            };
            return SystemTime{
                std::chrono::duration_cast<std::chrono::system_clock::duration>(
                    WindowsDuration{ unix_ticks }
                )
            };
#else
            static_cast<void>(path);
            return std::nullopt;
#endif
        }

        auto read_file_size(std::filesystem::directory_entry const& entry)
            -> std::optional<std::uintmax_t> {
            std::error_code size_error;
            auto const      size{ entry.file_size(size_error) };
            if (size_error) {
                return std::nullopt;
            }
            return size;
        }

        auto read_modified_time(std::filesystem::directory_entry const& entry)
            -> std::optional<std::filesystem::file_time_type> {
            std::error_code time_error;
            auto const      modified_time{ entry.last_write_time(time_error) };
            if (time_error) {
                return std::nullopt;
            }
            return modified_time;
        }

        auto format_file_size(std::optional<std::uintmax_t> size) -> std::string {
            if (!size) {
                return "未知";
            }

            constexpr std::array UNITS{ "B", "KB", "MB", "GB", "TB", "PB" };
            constexpr auto       UNIT_SIZE{ 1024.0 };

            auto       display_size{ static_cast<double>(*size) };
            auto       unit_index{ std::size_t{ 0 } };
            auto const last_unit_index{ UNITS.size() - 1 };
            while (display_size >= UNIT_SIZE && unit_index < last_unit_index) {
                display_size /= UNIT_SIZE;
                ++unit_index;
            }

            if (unit_index == 0) {
                return std::format("{} {}", *size, UNITS[unit_index]);
            }
            if (display_size >= 100.0) {
                return std::format("{:.0f} {}", display_size, UNITS[unit_index]);
            }
            if (display_size >= 10.0) {
                return std::format("{:.1f} {}", display_size, UNITS[unit_index]);
            }
            return std::format("{:.2f} {}", display_size, UNITS[unit_index]);
        }

        auto format_event_time(SystemTime time) -> std::string {
            auto const whole_seconds{ std::chrono::floor<std::chrono::seconds>(time) };
            auto const milliseconds{
                std::chrono::duration_cast<std::chrono::milliseconds>(time - whole_seconds)
                    .count()
            };
            auto const time_value{ std::chrono::system_clock::to_time_t(whole_seconds) };
            std::tm    local_time{};
#if defined(_WIN32)
            auto const conversion_failed{ localtime_s(&local_time, &time_value) != 0 };
#else
            auto const conversion_failed{ localtime_r(&time_value, &local_time) == nullptr };
#endif
            std::array<char, 20> local_time_text{};
            if (conversion_failed ||
                std::strftime(
                    local_time_text.data(),
                    local_time_text.size(),
                    "%Y-%m-%d %H:%M:%S",
                    &local_time
                ) == 0) {
                return std::format(
                    "{:%Y-%m-%d %H:%M:%S}.{:03}",
                    whole_seconds,
                    milliseconds
                );
            }
            return std::format("{}.{:03}", local_time_text.data(), milliseconds);
        }

        auto format_change_time(
            FileChangeStatus          status,
            std::optional<SystemTime> creation_time,
            SystemTime                event_time
        ) -> std::string {
            auto event_time_text{ format_event_time(event_time) };
            if (status != FileChangeStatus::Removed || !creation_time) {
                return event_time_text;
            }
            return std::format(
                "{} -> {}",
                format_event_time(*creation_time),
                event_time_text
            );
        }

        auto absolute_path_to_utf8(std::filesystem::path const& path) -> std::string {
            std::error_code absolute_error;
            auto const      absolute_path{ std::filesystem::absolute(path, absolute_error) };
            return path_to_utf8(absolute_error ? path : absolute_path);
        }

        auto format_change_path(std::string path, bool is_directory) -> std::string {
            if (is_directory && !path.empty()) {
                if (path.ends_with('/')) {
                    path.back() = '\\';
                } else if (!path.ends_with('\\')) {
                    path += '\\';
                }
            }
            return path;
        }

        auto add_file_change(
            std::vector<FileChange>& changes,
            FileChangeStatus         status,
            FileState const&         file
        ) -> void {
            changes.emplace_back(make_file_change(status, file));
        }

    } // namespace

    auto read_file_state(std::filesystem::path const& path) -> FileState {
        std::error_code                  entry_error;
        std::filesystem::directory_entry entry{ path, entry_error };
        if (entry_error) {
            return FileState{
                .creation_time = std::nullopt,
                .modified_time = std::nullopt,
                .size          = std::nullopt,
                .absolute_path = absolute_path_to_utf8(path),
                .is_directory  = false
            };
        }

        std::error_code type_error;
        auto const      is_directory{ entry.is_directory(type_error) && !type_error };

        return FileState{
            .creation_time = read_creation_time(path),
            .modified_time = read_modified_time(entry),
            .size          = is_directory ? std::nullopt : read_file_size(entry),
            .absolute_path = absolute_path_to_utf8(path),
            .is_directory  = is_directory
        };
    }

    auto scan_files(
        std::span<std::filesystem::path const> directories,
        std::stop_token                        stop_token
    ) -> std::vector<FileState> {
        std::vector<FileState> files;

        for (auto const& directory : directories) {
            if (stop_token.stop_requested()) {
                break;
            }

            std::error_code iteration_error;
            auto            iterator{
                std::filesystem::
                    recursive_directory_iterator{
                                                 directory,
                                                 std::filesystem::directory_options::skip_permission_denied,
                                                 iteration_error }
            };
            auto const end{ std::filesystem::recursive_directory_iterator{} };

            while (!stop_token.stop_requested() && !iteration_error && iterator != end) {
                std::error_code type_error;
                auto const      is_regular_file{ iterator->is_regular_file(type_error) };
                if (!type_error) {
                    auto const is_directory{ iterator->is_directory(type_error) };
                    if (!type_error && (is_regular_file || is_directory)) {
                        files.emplace_back(read_file_state(iterator->path()));
                    }
                }
                iterator.increment(iteration_error);
            }
        }

        if (stop_token.stop_requested()) {
            return {};
        }

        std::ranges::sort(files, {}, &FileState::absolute_path);
        auto const duplicates{ std::ranges::unique(files, {}, &FileState::absolute_path) };
        files.erase(duplicates.begin(), duplicates.end());
        return files;
    }

    auto scan_files(std::span<std::filesystem::path const> directories)
        -> std::vector<FileState> {
        return scan_files(directories, {});
    }

    auto detect_file_changes(
        std::span<FileState const> previous_files,
        std::span<FileState const> current_files
    ) -> std::vector<FileChange> {
        std::vector<FileChange> changes;
        auto                    previous_index{ std::size_t{ 0 } };
        auto                    current_index{ std::size_t{ 0 } };

        while (previous_index < previous_files.size() && current_index < current_files.size()) {
            auto const& previous{ previous_files[previous_index] };
            auto const& current{ current_files[current_index] };

            if (previous.absolute_path < current.absolute_path) {
                add_file_change(changes, FileChangeStatus::Removed, previous);
                ++previous_index;
                continue;
            }
            if (current.absolute_path < previous.absolute_path) {
                add_file_change(changes, FileChangeStatus::Added, current);
                ++current_index;
                continue;
            }
            if (!current.is_directory &&
                (previous.is_directory != current.is_directory ||
                 previous.modified_time != current.modified_time ||
                 previous.size != current.size)) {
                add_file_change(changes, FileChangeStatus::Modified, current);
            }
            ++previous_index;
            ++current_index;
        }

        for (; previous_index < previous_files.size(); ++previous_index) {
            add_file_change(
                changes,
                FileChangeStatus::Removed,
                previous_files[previous_index]
            );
        }
        for (; current_index < current_files.size(); ++current_index) {
            add_file_change(
                changes,
                FileChangeStatus::Added,
                current_files[current_index]
            );
        }
        return changes;
    }

    auto make_file_change(
        FileChangeStatus status,
        FileState const& file,
        std::string      previous_absolute_path
    ) -> FileChange {
        auto const event_time{ std::chrono::system_clock::now() };
        return FileChange{
            .time                   = format_change_time(status, file.creation_time, event_time),
            .status                 = status,
            .size                   = file.is_directory ? "-" : format_file_size(file.size),
            .absolute_path          = format_change_path(file.absolute_path, file.is_directory),
            .previous_absolute_path = format_change_path(
                std::move(previous_absolute_path),
                file.is_directory
            )
        };
    }

    auto file_change_status_text(FileChangeStatus status) -> std::string_view {
        switch (status) {
            case FileChangeStatus::Added:
                return "创建";
            case FileChangeStatus::Removed:
                return "删除";
            case FileChangeStatus::Modified:
                return "修改";
            case FileChangeStatus::Renamed:
                return "重命名";
        }
        return "未知";
    }

    auto path_to_utf8(std::filesystem::path const& path) -> std::string {
        auto const utf8_path{ path.u8string() };
        return { reinterpret_cast<char const*>(utf8_path.data()), utf8_path.size() };
    }

} // namespace file_monitor::core
