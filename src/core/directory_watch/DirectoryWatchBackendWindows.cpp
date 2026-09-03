#include <array>
#include <cstddef>
#include <format>
#include <optional>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include "DirectoryWatchBackend.hpp"
#include "core/Utils.hpp"

#ifndef NOMINMAX
    #define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace file_monitor::core::detail {
    namespace {

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
                std::filesystem::path          root,
                DirectoryWatchCallbacks const& callbacks
            )
                : m_root{ std::move(root) },
                  m_callbacks{ callbacks } {
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

            auto Start() -> bool {
                if (!initializeHandles()) {
                    return false;
                }
                if (auto const read_error{ beginRead() }) {
                    reportWindowsError("读取文件夹变更失败", *read_error);
                    return false;
                }

                try {
                    m_thread = std::jthread{ [this](std::stop_token stop_token) {
                        run(stop_token);
                    } };
                } catch (std::system_error const& error) {
                    cancelPendingRead();
                    m_callbacks.on_error(
                        std::format(
                            "无法创建文件夹监控线程 {}：{}",
                            utils::path_to_utf8(m_root),
                            error.what()
                        )
                    );
                    return false;
                } catch (...) {
                    cancelPendingRead();
                    throw;
                }
                return true;
            }

            auto RequestStop() noexcept -> void {
                m_thread.request_stop();
            }

        private:
            auto reportWindowsError(std::string_view operation, DWORD error) -> void {
                m_callbacks.on_error(
                    std::format(
                        "{} {}：{}",
                        operation,
                        utils::path_to_utf8(m_root),
                        std::system_category().message(static_cast<int>(error))
                    )
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

            auto beginRead() -> std::optional<DWORD> {
                ResetEvent(m_io_event.Get());
                m_overlapped        = {};
                m_overlapped.hEvent = m_io_event.Get();

                DWORD      ignored_bytes{};
                auto const read_started{
                    ReadDirectoryChangesW(
                        m_directory_handle.Get(),
                        m_buffer.data(),
                        static_cast<DWORD>(m_buffer.size()),
                        TRUE,
                        FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
                            FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE,
                        &ignored_bytes,
                        &m_overlapped,
                        nullptr
                    )
                };
                if (read_started) {
                    return std::nullopt;
                }

                auto const error{ GetLastError() };
                return error == ERROR_IO_PENDING ? std::nullopt : std::optional{ error };
            }

            auto cancelPendingRead() -> void {
                CancelIoEx(m_directory_handle.Get(), &m_overlapped);
                DWORD ignored_bytes{};
                GetOverlappedResult(
                    m_directory_handle.Get(),
                    &m_overlapped,
                    &ignored_bytes,
                    TRUE
                );
            }

            auto emitEvent(
                DirectoryWatchEventKind      kind,
                std::filesystem::path        path,
                std::stop_token              stop_token,
                std::filesystem::path const& previous_path = {}
            ) -> void {
                m_callbacks.on_event(
                    DirectoryWatchEvent{
                        .kind          = kind,
                        .path          = std::move(path),
                        .previous_path = previous_path },
                    stop_token
                );
            }

            auto processNotification(
                DWORD                                 action,
                std::filesystem::path const&          path,
                std::optional<std::filesystem::path>& pending_rename,
                std::stop_token                       stop_token
            ) -> void {
                if (pending_rename && action != FILE_ACTION_RENAMED_NEW_NAME) {
                    emitEvent(
                        DirectoryWatchEventKind::Removed,
                        *pending_rename,
                        stop_token
                    );
                    pending_rename.reset();
                }

                switch (action) {
                    case FILE_ACTION_ADDED:
                        emitEvent(DirectoryWatchEventKind::Added, path, stop_token);
                        break;
                    case FILE_ACTION_REMOVED:
                        emitEvent(DirectoryWatchEventKind::Removed, path, stop_token);
                        break;
                    case FILE_ACTION_MODIFIED:
                        emitEvent(DirectoryWatchEventKind::Modified, path, stop_token);
                        break;
                    case FILE_ACTION_RENAMED_OLD_NAME:
                        pending_rename = path;
                        break;
                    case FILE_ACTION_RENAMED_NEW_NAME:
                        if (pending_rename) {
                            emitEvent(
                                DirectoryWatchEventKind::PathChanged,
                                path,
                                stop_token,
                                *pending_rename
                            );
                            pending_rename.reset();
                        } else {
                            emitEvent(DirectoryWatchEventKind::Added, path, stop_token);
                        }
                        break;
                    default:
                        break;
                }
            }

            auto processNotifications(
                std::span<std::byte const>            bytes,
                std::optional<std::filesystem::path>& pending_rename,
                std::stop_token                       stop_token
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
                        pending_rename,
                        stop_token
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

            auto requestRescan(std::stop_token stop_token) -> void {
                m_callbacks.on_event(
                    DirectoryWatchEvent{
                        .kind = DirectoryWatchEventKind::RescanRequired },
                    stop_token
                );
            }

            auto run(std::stop_token stop_token) -> void {
                std::stop_callback stop_callback{ stop_token, [stop_event = m_stop_event.Get()] {
                                                     SetEvent(stop_event);
                                                 } };
                std::optional<std::filesystem::path> pending_rename;

                while (!stop_token.stop_requested()) {
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
                        cancelPendingRead();
                        return;
                    }
                    if (wait_result != WAIT_OBJECT_0 + 1) {
                        auto const wait_error{ GetLastError() };
                        cancelPendingRead();
                        reportWindowsError("等待文件夹变更失败", wait_error);
                        return;
                    }

                    DWORD      transferred_bytes{};
                    auto const read_completed{
                        GetOverlappedResult(
                            m_directory_handle.Get(),
                            &m_overlapped,
                            &transferred_bytes,
                            FALSE
                        )
                    };
                    if (!read_completed) {
                        auto const read_error{ GetLastError() };
                        if (read_error == ERROR_OPERATION_ABORTED &&
                            stop_token.stop_requested()) {
                            return;
                        }
                        if (read_error == ERROR_NOTIFY_ENUM_DIR) {
                            pending_rename.reset();
                            requestRescan(stop_token);
                        } else {
                            reportWindowsError("获取文件夹变更失败", read_error);
                            return;
                        }
                    } else if (transferred_bytes == 0) {
                        pending_rename.reset();
                        requestRescan(stop_token);
                    } else if (!processNotifications(
                                   std::span<std::byte const>{
                                       m_buffer.data(),
                                       transferred_bytes },
                                   pending_rename,
                                   stop_token
                               )) {
                        pending_rename.reset();
                        requestRescan(stop_token);
                    }

                    if (stop_token.stop_requested()) {
                        return;
                    }
                    if (auto const read_error{ beginRead() }) {
                        reportWindowsError("读取文件夹变更失败", *read_error);
                        return;
                    }
                }
            }

            std::filesystem::path          m_root;
            DirectoryWatchCallbacks const& m_callbacks;
            UniqueHandle                   m_directory_handle;
            UniqueHandle                   m_stop_event;
            UniqueHandle                   m_io_event;
            alignas(DWORD) std::array<std::byte, 64 * 1024> m_buffer{};
            OVERLAPPED   m_overlapped{};
            std::jthread m_thread;
        };

    } // namespace

    struct DirectoryWatchBackend::Impl {
        explicit Impl(DirectoryWatchCallbacks watch_callbacks)
            : callbacks{ std::move(watch_callbacks) } {
        }

        DirectoryWatchCallbacks                        callbacks;
        std::vector<std::unique_ptr<DirectoryWatcher>> watchers;
    };

    DirectoryWatchBackend::DirectoryWatchBackend(DirectoryWatchCallbacks callbacks)
        : m_impl{ std::make_unique<Impl>(std::move(callbacks)) } {
    }

    DirectoryWatchBackend::~DirectoryWatchBackend() {
        RequestStop();
        m_impl->watchers.clear();
    }

    auto DirectoryWatchBackend::Start(
        std::span<std::filesystem::path const> directories
    ) -> void {
        m_impl->watchers.reserve(directories.size());
        for (auto const& directory : directories) {
            auto watcher{
                std::make_unique<DirectoryWatcher>(directory, m_impl->callbacks)
            };
            if (watcher->Start()) {
                m_impl->watchers.emplace_back(std::move(watcher));
            }
        }
    }

    auto DirectoryWatchBackend::RequestStop() noexcept -> void {
        for (auto& watcher : m_impl->watchers) {
            watcher->RequestStop();
        }
    }

} // namespace file_monitor::core::detail
