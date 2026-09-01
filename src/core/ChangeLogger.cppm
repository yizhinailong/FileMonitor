module;

#include <expected>
#include <filesystem>
#include <memory>
#include <span>
#include <string>

export module core:change_logger;

import :file_system;

export namespace file_monitor::core {

    class ChangeLogger final {
    public:
        explicit ChangeLogger(std::filesystem::path log_directory);
        ~ChangeLogger();

        ChangeLogger(ChangeLogger const&)                    = delete;
        ChangeLogger(ChangeLogger&&)                         = delete;
        auto operator=(ChangeLogger const&) -> ChangeLogger& = delete;
        auto operator=(ChangeLogger&&) -> ChangeLogger&      = delete;

        auto Write(std::span<FileChange const> changes)
            -> std::expected<void, std::string>;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };

} // namespace file_monitor::core
