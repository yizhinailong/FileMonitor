export module file_monitor.core:directory_monitor;

import std;
export import :file_system;
import :directory_watch_backend;
import :utils;

namespace file_monitor::core {

    // Not in the anonymous namespace: DirectoryMonitor, which is part of the
    // module interface, holds a MonitorState.
    using FileCache = std::map<std::string, FileState, std::less<>>;

    struct MonitorState {
        std::mutex mutex;
        std::condition_variable initialized;
        std::vector<std::filesystem::path> excluded_directories;
        FileCache files;
        std::vector<FileChange> changes;
        std::string error_message;
        std::vector<std::filesystem::path> initializing_directories;
    };

    namespace {

        FileCache build_file_cache(std::span<FileState const> files)
        {
            FileCache cache;
            for (auto const& file : files) {
                cache.insert_or_assign(file.absolute_path, file);
            }
            return cache;
        }

        bool path_is_regular_file(std::filesystem::path const& path)
        {
            std::error_code type_error;
            return std::filesystem::is_regular_file(path, type_error) && !type_error;
        }

        bool path_is_descendant(
            std::filesystem::path const& path,
            std::filesystem::path const& directory)
        {
            auto const relative_path{ path.lexically_relative(directory) };
            return !relative_path.empty() && relative_path != "." &&
                !relative_path.is_absolute() && *relative_path.begin() != "..";
        }

        bool path_is_excluded(
            std::filesystem::path const& path,
            std::span<std::filesystem::path const> excluded_directories)
        {
            return std::ranges::any_of(
                excluded_directories,
                [&path](std::filesystem::path const& excluded_directory) {
                    return utils::path_is_within(path, excluded_directory);
                });
        }

        std::filesystem::path normalize_directory(std::filesystem::path const& directory)
        {
            std::error_code absolute_error;
            auto normalized{ std::filesystem::absolute(directory, absolute_error) };
            if (absolute_error) {
                normalized = directory;
            }
            return normalized.lexically_normal();
        }

        std::vector<FileState> take_descendants(
            FileCache& files,
            std::filesystem::path const& directory)
        {
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

        void remap_descendants(
            FileCache& files,
            std::filesystem::path const& previous_directory,
            std::filesystem::path const& current_directory)
        {
            std::vector<FileState> remapped_files;
            for (auto file{ files.begin() }; file != files.end();) {
                auto const file_path{ utils::utf8_to_path(file->first) };
                if (!path_is_descendant(file_path, previous_directory)) {
                    ++file;
                    continue;
                }

                auto remapped_file{ std::move(file->second) };
                auto const relative_path{ file_path.lexically_relative(previous_directory) };
                remapped_file.absolute_path = utils::path_to_utf8(
                    current_directory / relative_path);
                remapped_files.emplace_back(std::move(remapped_file));
                file = files.erase(file);
            }

            for (auto& file : remapped_files) {
                auto const file_path{ file.absolute_path };
                files.insert_or_assign(file_path, std::move(file));
            }
        }

        void report_error(std::shared_ptr<MonitorState> const& state, std::string message)
        {
            auto const lock{ std::scoped_lock{ state->mutex } };
            if (!state->error_message.empty()) {
                state->error_message += '\n';
            }
            state->error_message += std::move(message);
        }

        std::unique_lock<std::mutex> wait_for_initialization(
            std::shared_ptr<MonitorState> const& state,
            std::filesystem::path const& directory)
        {
            auto lock{ std::unique_lock{ state->mutex } };
            state->initialized.wait(lock, [&state, &directory] {
                return !std::ranges::contains(state->initializing_directories, directory);
            });
            return lock;
        }

        void initialize_directory(
            std::shared_ptr<MonitorState> const& state,
            std::filesystem::path directory,
            std::stop_token stop_token)
        {
            std::array const directories{ directory };
            auto const files{
                scan_files(directories, state->excluded_directories, stop_token)
            };
            if (stop_token.stop_requested()) {
                return;
            }

            auto directory_files{ build_file_cache(files) };
            {
                auto const lock{ std::scoped_lock{ state->mutex } };
                auto const initialization{
                    std::ranges::find(state->initializing_directories, directory)
                };
                if (stop_token.stop_requested() ||
                    initialization == state->initializing_directories.end()) {
                    return;
                }

                state->files.merge(directory_files);
                state->initializing_directories.erase(initialization);
            }
            state->initialized.notify_all();
        }

        void record_added(
            std::shared_ptr<MonitorState> const& state,
            std::filesystem::path const& path,
            std::filesystem::path const& root)
        {
            if (path_is_excluded(path, state->excluded_directories)) {
                return;
            }

            auto file{ read_file_state(path) };
            auto lock{ wait_for_initialization(state, root) };
            if (state->files.contains(file.absolute_path)) {
                return;
            }
            auto const file_path{ file.absolute_path };
            state->changes.emplace_back(make_file_change(FileChangeStatus::Added, file));
            state->files.emplace(file_path, std::move(file));
        }

        void record_removed(
            std::shared_ptr<MonitorState> const& state,
            std::filesystem::path const& path,
            std::filesystem::path const& root)
        {
            if (path_is_excluded(path, state->excluded_directories)) {
                return;
            }

            auto const path_text{ read_file_state(path).absolute_path };
            auto lock{ wait_for_initialization(state, root) };
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
                        utils::utf8_to_path(removed_file.absolute_path))
                };
                state->changes.append_range(
                    descendants |
                    std::views::reverse |
                    std::views::transform([](FileState const& file) {
                        return make_file_change(FileChangeStatus::Removed, file);
                    }));
            }
            state->changes.emplace_back(make_file_change(FileChangeStatus::Removed, removed_file));
        }

        void record_modified(
            std::shared_ptr<MonitorState> const& state,
            std::filesystem::path const& path,
            std::filesystem::path const& root)
        {
            if (path_is_excluded(path, state->excluded_directories) ||
                !path_is_regular_file(path)) {
                return;
            }

            auto file{ read_file_state(path) };
            auto lock{ wait_for_initialization(state, root) };
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

        void record_path_changed(
            std::shared_ptr<MonitorState> const& state,
            std::filesystem::path const& previous_path,
            std::filesystem::path const& current_path,
            std::filesystem::path const& root)
        {
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
                record_removed(state, previous_path, root);
                return;
            }
            if (previous_is_excluded) {
                record_added(state, current_path, root);
                return;
            }

            auto const status{
                previous_path.parent_path() == current_path.parent_path() ? FileChangeStatus::Renamed : FileChangeStatus::Moved
            };
            auto current_file{ read_file_state(current_path) };
            auto const previous_path_text{ read_file_state(previous_path).absolute_path };
            auto lock{ wait_for_initialization(state, root) };
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
                        utils::utf8_to_path(current_file.absolute_path));
                }
                state->files.erase(previous);
            }

            state->changes.emplace_back(
                make_file_change(
                    status,
                    current_file,
                    previous_path_text));
            auto const current_path_text{ current_file.absolute_path };
            state->files.insert_or_assign(current_path_text, std::move(current_file));
        }

        void resynchronize(
            std::shared_ptr<MonitorState> const& state,
            std::filesystem::path const& root,
            std::stop_token stop_token)
        {
            auto lock{ wait_for_initialization(state, root) };
            std::array const directories{ root };
            auto const current_files{
                scan_files(
                    directories,
                    state->excluded_directories,
                    stop_token)
            };
            if (stop_token.stop_requested()) {
                return;
            }

            auto const previous_files{
                state->files |
                std::views::values |
                std::views::filter([&root](FileState const& file) {
                    auto const path{ utils::utf8_to_path(file.absolute_path) };
                    return path == root || path_is_descendant(path, root);
                }) |
                std::ranges::to<std::vector<FileState>>()
            };

            auto changes{ detect_file_changes(previous_files, current_files) };
            state->changes.append_range(changes | std::views::as_rvalue);
            std::erase_if(state->files, [&root](auto const& entry) {
                auto const path{ utils::utf8_to_path(entry.first) };
                return path == root || path_is_descendant(path, root);
            });
            auto current_file_cache{ build_file_cache(current_files) };
            state->files.merge(current_file_cache);
        }

        void handle_directory_watch_event(
            std::shared_ptr<MonitorState> const& state,
            detail::DirectoryWatchEvent const& event,
            std::stop_token stop_token)
        {
            switch (event.kind) {
                case detail::DirectoryWatchEventKind::Added:
                    record_added(state, event.path, event.root);
                    break;
                case detail::DirectoryWatchEventKind::Removed:
                    record_removed(state, event.path, event.root);
                    break;
                case detail::DirectoryWatchEventKind::Modified:
                    record_modified(state, event.path, event.root);
                    break;
                case detail::DirectoryWatchEventKind::PathChanged:
                    record_path_changed(state, event.previous_path, event.path, event.root);
                    break;
                case detail::DirectoryWatchEventKind::RescanRequired:
                    resynchronize(state, event.root, stop_token);
                    break;
            }
        }

    } // namespace

    class DirectoryMonitor final {
    public:
        DirectoryMonitor() = default;

        ~DirectoryMonitor()
        {
            Stop();
        }

        DirectoryMonitor(DirectoryMonitor const&) = delete;
        DirectoryMonitor(DirectoryMonitor&&) = delete;
        DirectoryMonitor& operator=(DirectoryMonitor const&) = delete;
        DirectoryMonitor& operator=(DirectoryMonitor&&) = delete;

        void Start(
            std::span<std::filesystem::path const> directories,
            std::span<std::filesystem::path const> excluded_directories = {})
        {
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
                auto const lock{ std::scoped_lock{ m_state->mutex } };
                m_state->excluded_directories = std::move(normalized_excluded_directories);
                m_state->files = {};
                m_state->changes = {};
                m_state->error_message = {};
                m_state->initializing_directories = normalized_directories;
            }

            if (!normalized_directories.empty()) {
                m_watch_backend = std::make_unique<detail::DirectoryWatchBackend>(
                    detail::DirectoryWatchCallbacks{
                        .on_event = [state = m_state](
                                        detail::DirectoryWatchEvent event,
                                        std::stop_token stop_token) { handle_directory_watch_event(state, event, stop_token); },
                        .on_error = [state = m_state](std::string message) { report_error(state, std::move(message)); } });
                m_watch_backend->Start(normalized_directories);
            }

            m_initialization_threads.reserve(normalized_directories.size());
            for (auto const& directory : normalized_directories) {
                m_initialization_threads.emplace_back(
                    [state = m_state, directory](std::stop_token stop_token) mutable {
                        initialize_directory(state, std::move(directory), stop_token);
                    });
            }
            if (normalized_directories.empty()) {
                m_state->initialized.notify_all();
            }
        }

        void Stop()
        {
            for (auto& thread : m_initialization_threads) {
                thread.request_stop();
            }
            if (m_watch_backend) {
                m_watch_backend->RequestStop();
            }
            {
                auto const lock{ std::scoped_lock{ m_state->mutex } };
                m_state->initializing_directories.clear();
            }
            m_state->initialized.notify_all();
            m_watch_backend.reset();
            m_initialization_threads.clear();
        }

        std::vector<FileChange> TakeChanges()
        {
            auto const lock{ std::scoped_lock{ m_state->mutex } };
            std::vector<FileChange> changes;
            changes.swap(m_state->changes);
            return changes;
        }

        std::string TakeError()
        {
            auto const lock{ std::scoped_lock{ m_state->mutex } };
            std::string error_message;
            error_message.swap(m_state->error_message);
            return error_message;
        }

    private:
        std::shared_ptr<MonitorState> m_state{
            std::make_shared<MonitorState>()
        };
        std::vector<std::jthread> m_initialization_threads;
        std::unique_ptr<detail::DirectoryWatchBackend> m_watch_backend;
    };

} // namespace file_monitor::core
