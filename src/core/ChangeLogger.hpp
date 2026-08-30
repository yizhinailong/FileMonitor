#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <span>
#include <string>

#include "FileSystem.hpp"

namespace file_monitor::core {

    class ChangeLogger final {
    public:
        explicit ChangeLogger(std::filesystem::path log_directory);
        ~ChangeLogger();

        ChangeLogger(ChangeLogger const&)                    = delete;
        ChangeLogger(ChangeLogger&&)                         = delete;
        auto operator=(ChangeLogger const&) -> ChangeLogger& = delete;
        auto operator=(ChangeLogger&&) -> ChangeLogger&      = delete;

        auto Write(
            std::span<FileChange const> changes,
            std::uint64_t               first_record_number
        ) -> std::expected<void, std::string>;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };

} // namespace file_monitor::core
