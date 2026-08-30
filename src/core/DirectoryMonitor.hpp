#pragma once

#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "FileSystem.hpp"

namespace file_monitor::core {

    class DirectoryMonitor final {
    public:
        DirectoryMonitor();
        ~DirectoryMonitor();

        DirectoryMonitor(DirectoryMonitor const&)                    = delete;
        DirectoryMonitor(DirectoryMonitor&&)                         = delete;
        auto operator=(DirectoryMonitor const&) -> DirectoryMonitor& = delete;
        auto operator=(DirectoryMonitor&&) -> DirectoryMonitor&      = delete;

        auto Start(std::span<std::filesystem::path const> directories) -> void;
        auto Stop() -> void;
        auto TakeChanges() -> std::vector<FileChange>;
        auto TakeError() -> std::string;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };

} // namespace file_monitor::core
