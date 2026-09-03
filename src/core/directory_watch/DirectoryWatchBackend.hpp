#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <span>
#include <stop_token>
#include <string>

namespace file_monitor::core::detail {

    enum class DirectoryWatchEventKind {
        Added,
        Removed,
        Modified,
        PathChanged,
        RescanRequired
    };

    struct DirectoryWatchEvent {
        DirectoryWatchEventKind kind;
        std::filesystem::path   path;
        std::filesystem::path   previous_path;
    };

    struct DirectoryWatchCallbacks {
        std::function<void(DirectoryWatchEvent, std::stop_token)> on_event;
        std::function<void(std::string)>                          on_error;
    };

    class DirectoryWatchBackend final {
    public:
        explicit DirectoryWatchBackend(DirectoryWatchCallbacks callbacks);
        ~DirectoryWatchBackend();

        DirectoryWatchBackend(DirectoryWatchBackend const&)                    = delete;
        DirectoryWatchBackend(DirectoryWatchBackend&&)                         = delete;
        auto operator=(DirectoryWatchBackend const&) -> DirectoryWatchBackend& = delete;
        auto operator=(DirectoryWatchBackend&&) -> DirectoryWatchBackend&      = delete;

        auto Start(std::span<std::filesystem::path const> directories) -> void;
        auto RequestStop() noexcept -> void;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };

} // namespace file_monitor::core::detail
