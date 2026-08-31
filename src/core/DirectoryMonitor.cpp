#include "DirectoryMonitor.hpp"

#include <algorithm>
#include <array>
#include <condition_variable>
#include <cstddef>
#include <format>
#include <map>
#include <mutex>
#include <optional>
#include <ranges>
#include <stop_token>
#include <system_error>
#include <thread>
#include <utility>

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

        auto utf8_to_path(std::string_view path) -> std::filesystem::path {
            auto const* begin{ reinterpret_cast<char8_t const*>(path.data()) };
            return std::filesystem::path{
                std::u8string{ begin, begin + path.size() }
            };
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
                if (path_is_descendant(utf8_to_path(file->first), directory)) {
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
                auto const file_path{ utf8_to_path(file->first) };
                if (!path_is_descendant(file_path, previous_directory)) {
                    ++file;
                    continue;
                }

                auto       remapped_file{ std::move(file->second) };
                auto const relative_path{ file_path.lexically_relative(previous_directory) };
                remapped_file.absolute_path = path_to_utf8(current_directory / relative_path);
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
                        utf8_to_path(removed_file.absolute_path)
                    )
                };
                for (auto descendant{ descendants.rbegin() };
                     descendant != descendants.rend();
                     ++descendant) {
                    state->changes.emplace_back(
                        make_file_change(FileChangeStatus::Removed, *descendant)
                    );
                }
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
                        utf8_to_path(previous->second.absolute_path),
                        utf8_to_path(current_file.absolute_path)
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

            std::vector<FileState> previous_files;
            previous_files.reserve(state->files.size());
            for (auto const& [path, file] : state->files) {
                previous_files.emplace_back(file);
            }

            auto changes{ detect_file_changes(previous_files, current_files) };
            state->changes.append_range(changes | std::views::as_rvalue);
            state->files = build_file_cache(current_files);
        }

#if defined(_WIN32)

        class UniqueHandle final {
        public:
            UniqueHandle() = default;

            explicit UniqueHandle(HANDLE handle)
                : m_handle{ handle } {
            }

            ~UniqueHandle() {
                reset();
            }

            UniqueHandle(UniqueHandle const&)                    = delete;
            auto operator=(UniqueHandle const&) -> UniqueHandle& = delete;

            UniqueHandle(UniqueHandle&& other) noexcept
                : m_handle{ std::exchange(other.m_handle, nullptr) } {
            }

            auto operator=(UniqueHandle&& other) noexcept -> UniqueHandle& {
                if (this != &other) {
                    reset();
                    m_handle = std::exchange(other.m_handle, nullptr);
                }
                return *this;
            }

            auto Get() const -> HANDLE {
                return m_handle;
            }

            explicit operator bool() const {
                return m_handle != nullptr && m_handle != INVALID_HANDLE_VALUE;
            }

        private:
            auto reset() -> void {
                if (*this) {
                    CloseHandle(m_handle);
                }
                m_handle = nullptr;
            }

            HANDLE m_handle{ nullptr };
        };

        class DirectoryWatcher final {
        public:
            DirectoryWatcher(
                std::filesystem::path         root,
                std::shared_ptr<MonitorState> state
            )
                : m_root{ std::move(root) },
                  m_state{ std::move(state) } {
                m_thread = std::jthread{ [this](std::stop_token stop_token) {
                    run(stop_token);
                } };
            }

            ~DirectoryWatcher() {
                if (m_thread.joinable()) {
                    RequestStop();
                    m_thread.join();
                }
            }

            DirectoryWatcher(DirectoryWatcher const&)                    = delete;
            DirectoryWatcher(DirectoryWatcher&&)                         = delete;
            auto operator=(DirectoryWatcher const&) -> DirectoryWatcher& = delete;
            auto operator=(DirectoryWatcher&&) -> DirectoryWatcher&      = delete;

            auto RequestStop() -> void {
                m_thread.request_stop();
            }

        private:
            auto reportWindowsError(std::string_view operation, DWORD error) -> void {
                report_error(
                    m_state,
                    std::format(
                        "{} {}：{}",
                        operation,
                        path_to_utf8(m_root),
                        std::system_category().message(static_cast<int>(error))
                    )
                );
            }

            auto cancelPendingRead(OVERLAPPED& overlapped) -> void {
                CancelIoEx(m_directory_handle.Get(), &overlapped);
                DWORD ignored_bytes{};
                GetOverlappedResult(
                    m_directory_handle.Get(),
                    &overlapped,
                    &ignored_bytes,
                    TRUE
                );
            }

            auto initializeHandles() -> bool {
                m_directory_handle = UniqueHandle{
                    CreateFileW(
                        m_root.c_str(),
                        FILE_LIST_DIRECTORY,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                        nullptr,
                        OPEN_EXISTING,
                        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                        nullptr
                    )
                };
                if (!m_directory_handle) {
                    reportWindowsError("无法打开文件夹监控", GetLastError());
                    return false;
                }

                m_stop_event = UniqueHandle{ CreateEventW(nullptr, TRUE, FALSE, nullptr) };
                if (!m_stop_event) {
                    reportWindowsError("无法创建监控停止事件", GetLastError());
                    return false;
                }

                m_io_event = UniqueHandle{ CreateEventW(nullptr, TRUE, FALSE, nullptr) };
                if (!m_io_event) {
                    reportWindowsError("无法创建监控读取事件", GetLastError());
                    return false;
                }
                return true;
            }

            auto processNotification(
                DWORD                                 action,
                std::filesystem::path const&          path,
                std::optional<std::filesystem::path>& pending_rename
            ) -> void {
                if (pending_rename && action != FILE_ACTION_RENAMED_NEW_NAME) {
                    record_removed(m_state, *pending_rename);
                    pending_rename.reset();
                }

                switch (action) {
                    case FILE_ACTION_ADDED:
                        record_added(m_state, path);
                        break;
                    case FILE_ACTION_REMOVED:
                        record_removed(m_state, path);
                        break;
                    case FILE_ACTION_MODIFIED:
                        record_modified(m_state, path);
                        break;
                    case FILE_ACTION_RENAMED_OLD_NAME:
                        pending_rename = path;
                        break;
                    case FILE_ACTION_RENAMED_NEW_NAME:
                        if (pending_rename) {
                            record_path_changed(m_state, *pending_rename, path);
                            pending_rename.reset();
                        } else {
                            record_added(m_state, path);
                        }
                        break;
                    default:
                        break;
                }
            }

            auto processNotifications(
                std::span<std::byte const>            bytes,
                std::optional<std::filesystem::path>& pending_rename
            ) -> bool {
                constexpr auto HEADER_SIZE{ offsetof(FILE_NOTIFY_INFORMATION, FileName) };
                auto           offset{ std::size_t{ 0 } };

                while (offset < bytes.size()) {
                    auto const remaining{ bytes.size() - offset };
                    if (remaining < HEADER_SIZE) {
                        return false;
                    }

                    auto const* notification{
                        reinterpret_cast<FILE_NOTIFY_INFORMATION const*>(bytes.data() + offset)
                    };
                    if (notification->FileNameLength > remaining - HEADER_SIZE ||
                        notification->FileNameLength % sizeof(wchar_t) != 0) {
                        return false;
                    }

                    auto const character_count{
                        static_cast<std::size_t>(notification->FileNameLength) / sizeof(wchar_t)
                    };
                    auto const relative_path{
                        std::wstring{ notification->FileName, character_count }
                    };
                    processNotification(
                        notification->Action,
                        (m_root / relative_path).lexically_normal(),
                        pending_rename
                    );

                    if (notification->NextEntryOffset == 0) {
                        return true;
                    }
                    if (notification->NextEntryOffset < HEADER_SIZE ||
                        notification->NextEntryOffset > remaining) {
                        return false;
                    }
                    offset += notification->NextEntryOffset;
                }
                return true;
            }

            auto run(std::stop_token stop_token) -> void {
                if (!initializeHandles()) {
                    initialize_directory(m_state, m_root, stop_token);
                    return;
                }

                std::stop_callback stop_callback{ stop_token, [stop_event = m_stop_event.Get()] {
                                                     SetEvent(stop_event);
                                                 } };
                alignas(DWORD) std::array<std::byte, 64 * 1024> buffer{};
                std::optional<std::filesystem::path>            pending_rename;
                auto                                            initialized{ false };

                while (!stop_token.stop_requested()) {
                    ResetEvent(m_io_event.Get());
                    OVERLAPPED overlapped{};
                    overlapped.hEvent = m_io_event.Get();
                    DWORD      ignored_bytes{};
                    auto const read_started{
                        ReadDirectoryChangesW(
                            m_directory_handle.Get(),
                            buffer.data(),
                            static_cast<DWORD>(buffer.size()),
                            TRUE,
                            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
                                FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE,
                            &ignored_bytes,
                            &overlapped,
                            nullptr
                        )
                    };
                    auto const read_error{ read_started ? ERROR_SUCCESS : GetLastError() };
                    if (!read_started && read_error != ERROR_IO_PENDING) {
                        if (!initialized) {
                            initialize_directory(m_state, m_root, stop_token);
                        }
                        reportWindowsError("读取文件夹变更失败", read_error);
                        return;
                    }

                    if (!initialized) {
                        initialize_directory(m_state, m_root, stop_token);
                        initialized = true;
                        if (stop_token.stop_requested()) {
                            cancelPendingRead(overlapped);
                            return;
                        }
                    }

                    std::array const wait_handles{ m_stop_event.Get(), m_io_event.Get() };
                    auto const       wait_result{
                        WaitForMultipleObjects(
                            static_cast<DWORD>(wait_handles.size()),
                            wait_handles.data(),
                            FALSE,
                            INFINITE
                        )
                    };
                    if (wait_result == WAIT_OBJECT_0) {
                        cancelPendingRead(overlapped);
                        return;
                    }
                    if (wait_result != WAIT_OBJECT_0 + 1) {
                        auto const wait_error{ GetLastError() };
                        cancelPendingRead(overlapped);
                        reportWindowsError("等待文件夹变更失败", wait_error);
                        return;
                    }

                    DWORD transferred_bytes{};
                    if (!GetOverlappedResult(
                            m_directory_handle.Get(),
                            &overlapped,
                            &transferred_bytes,
                            FALSE
                        )) {
                        auto const read_error{ GetLastError() };
                        if (read_error == ERROR_OPERATION_ABORTED && stop_token.stop_requested()) {
                            return;
                        }
                        if (read_error == ERROR_NOTIFY_ENUM_DIR) {
                            pending_rename.reset();
                            resynchronize(m_state, stop_token);
                            continue;
                        }
                        reportWindowsError("获取文件夹变更失败", read_error);
                        return;
                    }

                    if (transferred_bytes == 0) {
                        pending_rename.reset();
                        resynchronize(m_state, stop_token);
                        continue;
                    }
                    if (!processNotifications(
                            std::span<std::byte const>{ buffer.data(), transferred_bytes },
                            pending_rename
                        )) {
                        pending_rename.reset();
                        resynchronize(m_state, stop_token);
                    }
                }
            }

            std::filesystem::path         m_root;
            std::shared_ptr<MonitorState> m_state;
            UniqueHandle                  m_directory_handle;
            UniqueHandle                  m_stop_event;
            UniqueHandle                  m_io_event;
            std::jthread                  m_thread;
        };

#endif

    } // namespace

    struct DirectoryMonitor::Impl {
        std::shared_ptr<MonitorState> state{ std::make_shared<MonitorState>() };
        std::vector<std::jthread>     initialization_threads;
#if defined(_WIN32)
        std::vector<std::unique_ptr<DirectoryWatcher>> watchers;
#endif
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

#if defined(_WIN32)
        for (auto const& directory : normalized_directories) {
            m_impl->watchers.emplace_back(
                std::make_unique<DirectoryWatcher>(directory, m_impl->state)
            );
        }
#else
        if (!normalized_directories.empty()) {
            report_error(m_impl->state, "当前平台暂不支持系统文件变更通知");
        }
        m_impl->initialization_threads.reserve(normalized_directories.size());
        for (auto const& directory : normalized_directories) {
            m_impl->initialization_threads.emplace_back(
                [state = m_impl->state, directory](std::stop_token stop_token) mutable {
                    initialize_directory(state, std::move(directory), stop_token);
                }
            );
        }
#endif
        if (normalized_directories.empty()) {
            m_impl->state->initialized.notify_all();
        }
    }

    auto DirectoryMonitor::Stop() -> void {
        for (auto& thread : m_impl->initialization_threads) {
            thread.request_stop();
        }
#if defined(_WIN32)
        for (auto& watcher : m_impl->watchers) {
            watcher->RequestStop();
        }
#endif
        {
            auto const lock{ std::scoped_lock{ m_impl->state->mutex } };
            m_impl->state->initializing            = false;
            m_impl->state->pending_initializations = 0;
        }
        m_impl->state->initialized.notify_all();
#if defined(_WIN32)
        m_impl->watchers.clear();
#endif
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
