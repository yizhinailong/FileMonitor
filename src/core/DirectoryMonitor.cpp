#include "DirectoryMonitor.hpp"

#include <array>
#include <condition_variable>
#include <cstddef>
#include <format>
#include <map>
#include <mutex>
#include <optional>
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
            FileCache                          files;
            std::vector<FileChange>            changes;
            std::string                        error_message;
            bool                               initializing{ false };
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

        auto record_added(
            std::shared_ptr<MonitorState> const& state,
            std::filesystem::path const&         path
        ) -> void {
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
            auto const path_text{ read_file_state(path).absolute_path };
            auto       lock{ wait_for_initialization(state) };
            auto const file{ state->files.find(path_text) };
            if (file == state->files.end()) {
                return;
            }
            state->changes.emplace_back(make_file_change(FileChangeStatus::Removed, file->second));
            state->files.erase(file);
        }

        auto record_modified(
            std::shared_ptr<MonitorState> const& state,
            std::filesystem::path const&         path
        ) -> void {
            if (!path_is_regular_file(path)) {
                return;
            }

            auto       file{ read_file_state(path) };
            auto       lock{ wait_for_initialization(state) };
            auto const previous{ state->files.find(file.absolute_path) };
            if (previous != state->files.end() &&
                previous->second.modified_time == file.modified_time &&
                previous->second.size == file.size) {
                return;
            }
            auto const file_path{ file.absolute_path };
            state->changes.emplace_back(make_file_change(FileChangeStatus::Modified, file));
            state->files.insert_or_assign(file_path, std::move(file));
        }

        auto record_renamed(
            std::shared_ptr<MonitorState> const& state,
            std::filesystem::path const&         previous_path,
            std::filesystem::path const&         current_path
        ) -> void {
            auto       current_file{ read_file_state(current_path) };
            auto const previous_path_text{ read_file_state(previous_path).absolute_path };
            auto       lock{ wait_for_initialization(state) };
            auto const previous{ state->files.find(previous_path_text) };

            if (previous == state->files.end() &&
                state->files.contains(current_file.absolute_path)) {
                return;
            }
            if (previous != state->files.end()) {
                if (!current_file.modified_time) {
                    current_file.modified_time = previous->second.modified_time;
                }
                if (!current_file.size) {
                    current_file.size = previous->second.size;
                }
                state->files.erase(previous);
            }

            state->changes.emplace_back(
                make_file_change(
                    FileChangeStatus::Renamed,
                    current_file,
                    previous_path_text
                )
            );
            auto const current_path_text{ current_file.absolute_path };
            state->files.insert_or_assign(current_path_text, std::move(current_file));
        }

        auto resynchronize(std::shared_ptr<MonitorState> const& state) -> void {
            auto       lock{ wait_for_initialization(state) };
            auto const current_files{ scan_files(state->directories) };

            std::vector<FileState> previous_files;
            previous_files.reserve(state->files.size());
            for (auto const& [path, file] : state->files) {
                previous_files.emplace_back(file);
            }

            auto changes{ detect_file_changes(previous_files, current_files) };
            for (auto& change : changes) {
                state->changes.emplace_back(std::move(change));
            }
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
                  m_state{ std::move(state) },
                  m_directory_handle{
                      CreateFileW(
                          m_root.c_str(),
                          FILE_LIST_DIRECTORY,
                          FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                          nullptr,
                          OPEN_EXISTING,
                          FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                          nullptr
                      )
                  },
                  m_stop_event{ CreateEventW(nullptr, TRUE, FALSE, nullptr) },
                  m_io_event{ CreateEventW(nullptr, TRUE, FALSE, nullptr) },
                  m_ready_event{ CreateEventW(nullptr, TRUE, FALSE, nullptr) } {
                if (!m_directory_handle || !m_stop_event || !m_io_event || !m_ready_event) {
                    reportWindowsError("无法启动文件夹监控", GetLastError());
                    return;
                }
                m_thread = std::jthread{ [this](std::stop_token stop_token) {
                    run(stop_token);
                } };
                if (WaitForSingleObject(m_ready_event.Get(), INFINITE) != WAIT_OBJECT_0) {
                    reportWindowsError("等待文件夹监控启动失败", GetLastError());
                }
            }

            ~DirectoryWatcher() {
                if (m_thread.joinable()) {
                    m_thread.request_stop();
                    m_thread.join();
                }
            }

            DirectoryWatcher(DirectoryWatcher const&)                    = delete;
            DirectoryWatcher(DirectoryWatcher&&)                         = delete;
            auto operator=(DirectoryWatcher const&) -> DirectoryWatcher& = delete;
            auto operator=(DirectoryWatcher&&) -> DirectoryWatcher&      = delete;

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
                            record_renamed(m_state, *pending_rename, path);
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
                std::stop_callback stop_callback{ stop_token, [stop_event = m_stop_event.Get()] {
                                                     SetEvent(stop_event);
                                                 } };
                alignas(DWORD) std::array<std::byte, 64 * 1024> buffer{};
                std::optional<std::filesystem::path>            pending_rename;
                auto                                            ready_signaled{ false };

                auto const signal_ready{ [this, &ready_signaled] {
                    if (!ready_signaled) {
                        SetEvent(m_ready_event.Get());
                        ready_signaled = true;
                    }
                } };

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
                            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_SIZE |
                                FILE_NOTIFY_CHANGE_LAST_WRITE,
                            &ignored_bytes,
                            &overlapped,
                            nullptr
                        )
                    };
                    auto const read_error{ read_started ? ERROR_SUCCESS : GetLastError() };
                    if (!read_started && read_error != ERROR_IO_PENDING) {
                        signal_ready();
                        reportWindowsError("读取文件夹变更失败", read_error);
                        return;
                    }
                    signal_ready();

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
                            resynchronize(m_state);
                            continue;
                        }
                        reportWindowsError("获取文件夹变更失败", read_error);
                        return;
                    }

                    if (transferred_bytes == 0) {
                        pending_rename.reset();
                        resynchronize(m_state);
                        continue;
                    }
                    if (!processNotifications(
                            std::span<std::byte const>{ buffer.data(), transferred_bytes },
                            pending_rename
                        )) {
                        pending_rename.reset();
                        resynchronize(m_state);
                    }
                }
            }

            std::filesystem::path         m_root;
            std::shared_ptr<MonitorState> m_state;
            UniqueHandle                  m_directory_handle;
            UniqueHandle                  m_stop_event;
            UniqueHandle                  m_io_event;
            UniqueHandle                  m_ready_event;
            std::jthread                  m_thread;
        };

#endif

    } // namespace

    struct DirectoryMonitor::Impl {
        std::shared_ptr<MonitorState> state{ std::make_shared<MonitorState>() };
#if defined(_WIN32)
        std::vector<std::unique_ptr<DirectoryWatcher>> watchers;
#endif
    };

    DirectoryMonitor::DirectoryMonitor()
        : m_impl{ std::make_unique<Impl>() } {
    }

    DirectoryMonitor::~DirectoryMonitor() = default;

    auto DirectoryMonitor::Start(
        std::span<std::filesystem::path const> directories
    ) -> void {
        Stop();

        {
            auto const lock{ std::scoped_lock{ m_impl->state->mutex } };
            m_impl->state->directories.assign(directories.begin(), directories.end());
            m_impl->state->files         = {};
            m_impl->state->changes       = {};
            m_impl->state->error_message = {};
            m_impl->state->initializing  = true;
        }

#if defined(_WIN32)
        for (auto const& directory : directories) {
            m_impl->watchers.emplace_back(
                std::make_unique<DirectoryWatcher>(directory, m_impl->state)
            );
        }
#else
        if (!directories.empty()) {
            report_error(m_impl->state, "当前平台暂不支持系统文件变更通知");
        }
#endif

        auto const files{ scan_files(directories) };
        {
            auto const lock{ std::scoped_lock{ m_impl->state->mutex } };
            m_impl->state->files        = build_file_cache(files);
            m_impl->state->initializing = false;
        }
        m_impl->state->initialized.notify_all();
    }

    auto DirectoryMonitor::Stop() -> void {
#if defined(_WIN32)
        m_impl->watchers.clear();
#endif
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
