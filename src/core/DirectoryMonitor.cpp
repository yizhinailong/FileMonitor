#include "DirectoryMonitor.hpp"

#include <algorithm>
#include <array>
#include <condition_variable>
#include <cstddef>
#include <map>
#include <mutex>
#include <ranges>
#include <stop_token>
#include <thread>
#include <utility>

#include "Utils.hpp"
#include "directory_watch/DirectoryWatchBackend.hpp"

namespace file_monitor::core {
    namespace {

        using FileCache = std::map<std::string, FileState, std::less<>>;

        struct MonitorState {
            std::mutex                         mutex;
            std::condition_variable            initialized;
            std::vector<std::filesystem::path> directories;
            std::vector<std::filesystem::path> excluded_directories;
            FileCache                          files;
            std::vector<FileChange>            changes;
            std::string                        error_message;
            bool                               initializing{ false };
            std::size_t                        pending_initializations{ 0 };
        };

        auto build_file_cache(std::span<FileState const> files) -> FileCache {
            FileCache cache;
            for (auto const& file : files) {
                cache.insert_or_assign(file.absolute_path, file);
            }
            return cache;
        }

        auto path_is_regular_file(std::filesystem::path const& path) -> bool {
            std::error_code type_error;
            return std::filesystem::is_regular_file(path, type_error) && !type_error;
        }

        auto path_is_descendant(
            std::filesystem::path const& path,
            std::filesystem::path const& directory
        ) -> bool {
            auto const relative_path{ path.lexically_relative(directory) };
            return !relative_path.empty() && relative_path != "." &&
                   !relative_path.is_absolute() && *relative_path.begin() != "..";
        }

        auto path_is_excluded(
            std::filesystem::path const&           path,
            std::span<std::filesystem::path const> excluded_directories
        ) -> bool {
            return std::ranges::any_of(
                excluded_directories,
                [&path](std::filesystem::path const& excluded_directory) {
                    return path == excluded_directory ||
                           path_is_descendant(path, excluded_directory);
                }
            );
        }

        auto normalize_directory(std::filesystem::path const& directory)
            -> std::filesystem::path {
            std::error_code absolute_error;
            auto            normalized{ std::filesystem::absolute(directory, absolute_error) };
            if (absolute_error) {
                normalized = directory;
            }
            return normalized.lexically_normal();
        }

        auto take_descendants(FileCache& files, std::filesystem::path const& directory)
            -> std::vector<FileState> {
            std::vector<FileState> descendants;
            for (auto file{ files.begin() }; file != files.end();) {
                if (path_is_descendant(utils::utf8_to_path(file->first), directory)) {
                    descendants.emplace_back(std::move(file->second));
                    file = files.erase(file);
                } else {
                    ++file;
                }
            }
            return descendants;
        }

        auto remap_descendants(
            FileCache&                   files,
            std::filesystem::path const& previous_directory,
            std::filesystem::path const& current_directory
        ) -> void {
            std::vector<FileState> remapped_files;
            for (auto file{ files.begin() }; file != files.end();) {
                auto const file_path{ utils::utf8_to_path(file->first) };
                if (!path_is_descendant(file_path, previous_directory)) {
                    ++file;
                    continue;
                }

                auto       remapped_file{ std::move(file->second) };
                auto const relative_path{ file_path.lexically_relative(previous_directory) };
                remapped_file.absolute_path = utils::path_to_utf8(
                    current_directory / relative_path
                );
                remapped_files.emplace_back(std::move(remapped_file));
                file = files.erase(file);
            }

            for (auto& file : remapped_files) {
                auto const file_path{ file.absolute_path };
                files.insert_or_assign(file_path, std::move(file));
            }
        }

        auto report_error(
            std::shared_ptr<MonitorState> const& state,
            std::string                          message
        ) -> void {
            auto const lock{ std::scoped_lock{ state->mutex } };
            if (!state->error_message.empty()) {
                state->error_message += '\n';
            }
            state->error_message += std::move(message);
        }

        auto wait_for_initialization(std::shared_ptr<MonitorState> const& state)
            -> std::unique_lock<std::mutex> {
            auto lock{ std::unique_lock{ state->mutex } };
            state->initialized.wait(lock, [&state] {
                return !state->initializing;
            });
            return lock;
        }

        auto initialize_directory(
            std::shared_ptr<MonitorState> const& state,
            std::filesystem::path                directory,
            std::stop_token                      stop_token
        ) -> void {
            std::array const directories{ std::move(directory) };
            auto const       files{
                scan_files(directories, state->excluded_directories, stop_token)
            };
            if (stop_token.stop_requested()) {
                return;
            }

            auto directory_files{ build_file_cache(files) };
            auto initialization_completed{ false };
            {
                auto const lock{ std::scoped_lock{ state->mutex } };
                if (stop_token.stop_requested() || state->pending_initializations == 0) {
                    return;
                }

                state->files.merge(directory_files);
                --state->pending_initializations;
                initialization_completed = state->pending_initializations == 0;
                if (initialization_completed) {
                    state->initializing = false;
                }
            }
            if (initialization_completed) {
                state->initialized.notify_all();
            }
        }

        auto record_added(
            std::shared_ptr<MonitorState> const& state,
            std::filesystem::path const&         path
        ) -> void {
            if (path_is_excluded(path, state->excluded_directories)) {
                return;
            }

            auto file{ read_file_state(path) };
            auto lock{ wait_for_initialization(state) };
            if (state->files.contains(file.absolute_path)) {
                return;
            }
            auto const file_path{ file.absolute_path };
            state->changes.emplace_back(make_file_change(FileChangeStatus::Added, file));
            state->files.emplace(file_path, std::move(file));
        }

        auto record_removed(
            std::shared_ptr<MonitorState> const& state,
            std::filesystem::path const&         path
        ) -> void {
            if (path_is_excluded(path, state->excluded_directories)) {
                return;
            }

            auto const path_text{ read_file_state(path).absolute_path };
            auto       lock{ wait_for_initialization(state) };
            auto const file{ state->files.find(path_text) };
            if (file == state->files.end()) {
                return;
            }

            auto const removed_file{ file->second };
            state->files.erase(file);
            if (removed_file.is_directory) {
                auto const descendants{
                    take_descendants(
                        state->files,
                        utils::utf8_to_path(removed_file.absolute_path)
                    )
                };
                state->changes.append_range(
                    descendants |
                    std::views::reverse |
                    std::views::transform([](FileState const& file) {
                        return make_file_change(FileChangeStatus::Removed, file);
                    })
                );
            }
            state->changes.emplace_back(make_file_change(FileChangeStatus::Removed, removed_file));
        }

        auto record_modified(
            std::shared_ptr<MonitorState> const& state,
            std::filesystem::path const&         path
        ) -> void {
            if (path_is_excluded(path, state->excluded_directories) ||
                !path_is_regular_file(path)) {
                return;
            }

            auto       file{ read_file_state(path) };
            auto       lock{ wait_for_initialization(state) };
            auto const previous{ state->files.find(file.absolute_path) };
            if (previous != state->files.end() && !file.creation_time) {
                file.creation_time = previous->second.creation_time;
            }
            if (previous != state->files.end() &&
                previous->second.modified_time == file.modified_time &&
                previous->second.size == file.size) {
                return;
            }
            auto const file_path{ file.absolute_path };
            state->changes.emplace_back(make_file_change(FileChangeStatus::Modified, file));
            state->files.insert_or_assign(file_path, std::move(file));
        }

        auto record_path_changed(
            std::shared_ptr<MonitorState> const& state,
            std::filesystem::path const&         previous_path,
            std::filesystem::path const&         current_path
        ) -> void {
            auto const previous_is_excluded{
                path_is_excluded(previous_path, state->excluded_directories)
            };
            auto const current_is_excluded{
                path_is_excluded(current_path, state->excluded_directories)
            };
            if (previous_is_excluded && current_is_excluded) {
                return;
            }
            if (current_is_excluded) {
                record_removed(state, previous_path);
                return;
            }
            if (previous_is_excluded) {
                record_added(state, current_path);
                return;
            }

            auto const status{
                previous_path.parent_path() == current_path.parent_path() ? FileChangeStatus::Renamed : FileChangeStatus::Moved
            };
            auto       current_file{ read_file_state(current_path) };
            auto const previous_path_text{ read_file_state(previous_path).absolute_path };
            auto       lock{ wait_for_initialization(state) };
            auto const previous{ state->files.find(previous_path_text) };

            if (previous == state->files.end() &&
                state->files.contains(current_file.absolute_path)) {
                return;
            }
            if (previous != state->files.end()) {
                current_file.is_directory = previous->second.is_directory;
                if (!current_file.creation_time) {
                    current_file.creation_time = previous->second.creation_time;
                }
                if (!current_file.modified_time) {
                    current_file.modified_time = previous->second.modified_time;
                }
                if (!current_file.size) {
                    current_file.size = previous->second.size;
                }
                if (current_file.is_directory) {
                    remap_descendants(
                        state->files,
                        utils::utf8_to_path(previous->second.absolute_path),
                        utils::utf8_to_path(current_file.absolute_path)
                    );
                }
                state->files.erase(previous);
            }

            state->changes.emplace_back(
                make_file_change(
                    status,
                    current_file,
                    previous_path_text
                )
            );
            auto const current_path_text{ current_file.absolute_path };
            state->files.insert_or_assign(current_path_text, std::move(current_file));
        }

        auto resynchronize(
            std::shared_ptr<MonitorState> const& state,
            std::stop_token                      stop_token
        ) -> void {
            auto       lock{ wait_for_initialization(state) };
            auto const current_files{
                scan_files(
                    state->directories,
                    state->excluded_directories,
                    stop_token
                )
            };
            if (stop_token.stop_requested()) {
                return;
            }

            auto const previous_files{
                state->files |
                std::views::values |
                std::ranges::to<std::vector<FileState>>()
            };

            auto changes{ detect_file_changes(previous_files, current_files) };
            state->changes.append_range(changes | std::views::as_rvalue);
            state->files = build_file_cache(current_files);
        }

        auto handle_directory_watch_event(
            std::shared_ptr<MonitorState> const& state,
            detail::DirectoryWatchEvent const&   event,
            std::stop_token                      stop_token
        ) -> void {
            switch (event.kind) {
                case detail::DirectoryWatchEventKind::Added:
                    record_added(state, event.path);
                    break;
                case detail::DirectoryWatchEventKind::Removed:
                    record_removed(state, event.path);
                    break;
                case detail::DirectoryWatchEventKind::Modified:
                    record_modified(state, event.path);
                    break;
                case detail::DirectoryWatchEventKind::PathChanged:
                    record_path_changed(state, event.previous_path, event.path);
                    break;
                case detail::DirectoryWatchEventKind::RescanRequired:
                    resynchronize(state, stop_token);
                    break;
            }
        }

    } // namespace

    struct DirectoryMonitor::Impl {
        std::shared_ptr<MonitorState> state{
            std::make_shared<MonitorState>()
        };
        std::vector<std::jthread>                      initialization_threads;
        std::unique_ptr<detail::DirectoryWatchBackend> watch_backend;
    };

    DirectoryMonitor::DirectoryMonitor()
        : m_impl{ std::make_unique<Impl>() } {
    }

    DirectoryMonitor::~DirectoryMonitor() {
        Stop();
    }

    auto DirectoryMonitor::Start(
        std::span<std::filesystem::path const> directories,
        std::span<std::filesystem::path const> excluded_directories
    ) -> void {
        Stop();

        auto normalized_directories{
            directories |
            std::views::transform(normalize_directory) |
            std::ranges::to<std::vector<std::filesystem::path>>()
        };
        auto normalized_excluded_directories{
            excluded_directories |
            std::views::transform(normalize_directory) |
            std::ranges::to<std::vector<std::filesystem::path>>()
        };

        {
            auto const lock{ std::scoped_lock{ m_impl->state->mutex } };
            m_impl->state->directories = normalized_directories;
            m_impl->state->excluded_directories =
                std::move(normalized_excluded_directories);
            m_impl->state->files                   = {};
            m_impl->state->changes                 = {};
            m_impl->state->error_message           = {};
            m_impl->state->initializing            = !normalized_directories.empty();
            m_impl->state->pending_initializations = normalized_directories.size();
        }

        if (!normalized_directories.empty()) {
            m_impl->watch_backend = std::make_unique<detail::DirectoryWatchBackend>(
                detail::DirectoryWatchCallbacks{
                    .on_event = [state = m_impl->state](
                                    detail::DirectoryWatchEvent event,
                                    std::stop_token             stop_token
                                ) { handle_directory_watch_event(state, event, stop_token); },
                    .on_error = [state = m_impl->state](std::string message) { report_error(state, std::move(message)); } }
            );
            m_impl->watch_backend->Start(normalized_directories);
        }

        m_impl->initialization_threads.reserve(normalized_directories.size());
        for (auto const& directory : normalized_directories) {
            m_impl->initialization_threads.emplace_back(
                [state = m_impl->state, directory](std::stop_token stop_token) mutable {
                    initialize_directory(state, std::move(directory), stop_token);
                }
            );
        }
        if (normalized_directories.empty()) {
            m_impl->state->initialized.notify_all();
        }
    }

    auto DirectoryMonitor::Stop() -> void {
        for (auto& thread : m_impl->initialization_threads) {
            thread.request_stop();
        }
        if (m_impl->watch_backend) {
            m_impl->watch_backend->RequestStop();
        }
        {
            auto const lock{ std::scoped_lock{ m_impl->state->mutex } };
            m_impl->state->initializing            = false;
            m_impl->state->pending_initializations = 0;
        }
        m_impl->state->initialized.notify_all();
        m_impl->watch_backend.reset();
        m_impl->initialization_threads.clear();
    }

    auto DirectoryMonitor::TakeChanges() -> std::vector<FileChange> {
        auto const              lock{ std::scoped_lock{ m_impl->state->mutex } };
        std::vector<FileChange> changes;
        changes.swap(m_impl->state->changes);
        return changes;
    }

    auto DirectoryMonitor::TakeError() -> std::string {
        auto const  lock{ std::scoped_lock{ m_impl->state->mutex } };
        std::string error_message;
        error_message.swap(m_impl->state->error_message);
        return error_message;
    }

} // namespace file_monitor::core
