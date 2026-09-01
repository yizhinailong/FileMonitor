module;

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

export module core:configuration;

export namespace file_monitor::core {

    struct Configuration {
        std::vector<std::filesystem::path> directories;
        std::vector<std::filesystem::path> excluded_directories;
    };

    auto load_configuration(std::filesystem::path const& config_path)
        -> std::expected<Configuration, std::string>;
    auto save_configuration(
        std::filesystem::path const& config_path,
        Configuration const&         configuration
    ) -> std::expected<void, std::string>;

} // namespace file_monitor::core
