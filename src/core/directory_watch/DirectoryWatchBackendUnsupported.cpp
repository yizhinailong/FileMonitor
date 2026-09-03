#include <utility>

#include "DirectoryWatchBackend.hpp"

namespace file_monitor::core::detail {

    struct DirectoryWatchBackend::Impl {
        explicit Impl(DirectoryWatchCallbacks watch_callbacks)
            : callbacks{ std::move(watch_callbacks) } {
        }

        DirectoryWatchCallbacks callbacks;
    };

    DirectoryWatchBackend::DirectoryWatchBackend(DirectoryWatchCallbacks callbacks)
        : m_impl{ std::make_unique<Impl>(std::move(callbacks)) } {
    }

    DirectoryWatchBackend::~DirectoryWatchBackend() = default;

    auto DirectoryWatchBackend::Start(
        std::span<std::filesystem::path const> directories
    ) -> void {
        if (!directories.empty()) {
            m_impl->callbacks.on_error("当前平台暂不支持系统文件变更通知");
        }
    }

    auto DirectoryWatchBackend::RequestStop() noexcept -> void {
    }

} // namespace file_monitor::core::detail
