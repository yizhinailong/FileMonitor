module;

#include <algorithm>
#include <cstddef>
#include <exception>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

export module file_monitor.core:configuration;

import :utils;

export namespace file_monitor::core {

    struct Configuration {
        std::vector<std::filesystem::path> directories;
        std::vector<std::filesystem::path> excluded_directories;
    };

    auto load_configuration(std::filesystem::path const& config_path)
        -> std::expected<Configuration, std::string>;
    auto save_configuration(
        std::filesystem::path const& config_path,
        Configuration const& configuration
    ) -> std::expected<void, std::string>;

    auto add_directory(
        std::vector<std::filesystem::path>& directories,
        std::filesystem::path directory
    ) -> bool;
    auto add_directory_input(
        std::vector<std::filesystem::path>& directories,
        std::string_view input
    ) -> std::expected<void, std::string>;

    // Why a directory of the configuration has no effect, one entry per
    // directory; an empty entry means the directory is fine. A monitored
    // directory has none if it cannot be read as a directory or lies in another
    // monitored directory, an excluded directory if it lies in no monitored
    // directory. Both look at the file system, so call them when the
    // configuration changes rather than for every frame.
    auto monitored_directory_warnings(Configuration const& configuration) -> std::vector<std::string>;
    auto excluded_directory_warnings(Configuration const& configuration) -> std::vector<std::string>;

} // namespace file_monitor::core

namespace file_monitor::core {
    namespace {

        auto config_error(
            std::string_view operation,
            std::filesystem::path const& config_path,
            std::string_view detail
        ) -> std::string {
            return std::format(
                "{} {}：{}",
                operation,
                utils::path_to_utf8(config_path),
                detail
            );
        }

        auto load_directories(
            nlohmann::json const& config_data,
            std::string_view key,
            std::vector<std::filesystem::path>& directories,
            bool required
        ) -> std::expected<void, std::string> {
            auto const directory_values{ config_data.find(key) };
            if (directory_values == config_data.end()) {
                if (required) {
                    return std::unexpected{ std::format("{} 必须是数组", key) };
                }
                return {};
            }
            if (!directory_values->is_array()) {
                return std::unexpected{ std::format("{} 必须是数组", key) };
            }

            for (auto const& directory_value : *directory_values) {
                if (!directory_value.is_string()) {
                    return std::unexpected{
                        std::format("{} 中的文件夹路径必须是字符串", key)
                    };
                }

                auto const& directory_text{ directory_value.get_ref<std::string const&>() };
                if (directory_text.empty()) {
                    return std::unexpected{
                        std::format("{} 中的文件夹路径不能为空", key)
                    };
                }

                add_directory(directories, utils::utf8_to_path(directory_text));
            }
            return {};
        }

        // Directories are stored as absolute paths without a trailing separator,
        // so a relative input does not depend on the working directory of a later
        // run and "C:\a\" is not added next to "C:\a".
        auto normalize_directory(std::filesystem::path directory) -> std::filesystem::path {
            std::error_code absolute_error;
            auto absolute_directory{ std::filesystem::absolute(directory, absolute_error) };
            if (!absolute_error) {
                directory = std::move(absolute_directory);
            }
            directory = directory.lexically_normal();
            if (directory.has_relative_path() && !directory.has_filename()) {
                directory = directory.parent_path();
            }
            return directory;
        }

        auto monitored_directory_warning(
            std::vector<std::filesystem::path> const& directories,
            std::size_t index
        ) -> std::string {
            auto const& directory{ directories[index] };
            std::error_code status_error;
            auto const status{ std::filesystem::status(directory, status_error) };
            if (status.type() == std::filesystem::file_type::not_found) {
                return "不存在";
            }
            if (status_error) {
                return "无法访问";
            }
            if (!std::filesystem::is_directory(status)) {
                return "不是文件夹";
            }
            for (auto const [other_index, other] : std::views::enumerate(directories)) {
                if (static_cast<std::size_t>(other_index) != index &&
                    utils::path_is_within(directory, other)) {
                    return "已在其他监控文件夹中";
                }
            }
            return {};
        }

    } // namespace

    auto load_configuration(std::filesystem::path const& config_path)
        -> std::expected<Configuration, std::string> {
        std::error_code exists_error;
        auto const config_exists{ std::filesystem::exists(config_path, exists_error) };
        if (exists_error) {
            return std::unexpected{
                config_error("检查配置文件失败", config_path, exists_error.message())
            };
        }
        if (!config_exists) {
            return Configuration{};
        }

        std::ifstream input{ config_path, std::ios::binary };
        if (!input) {
            return std::unexpected{ config_error("打开配置文件失败", config_path, "无法读取") };
        }

        // nlohmann::json brace initialization would wrap the parsed value in an array.
        auto const config_data = nlohmann::json::parse(input, nullptr, false);
        if (input.bad()) {
            return std::unexpected{ config_error("读取配置文件失败", config_path, "读取错误") };
        }
        if (config_data.is_discarded()) {
            return std::unexpected{ config_error("解析配置文件失败", config_path, "JSON 格式无效") };
        }
        if (!config_data.is_object()) {
            return std::unexpected{ config_error("解析配置文件失败", config_path, "根节点必须是对象") };
        }

        Configuration configuration;
        auto const directories_result{
            load_directories(config_data, "directories", configuration.directories, true)
        };
        if (!directories_result) {
            return std::unexpected{
                config_error("解析配置文件失败", config_path, directories_result.error())
            };
        }
        auto const excluded_directories_result{
            load_directories(
                config_data,
                "excluded_directories",
                configuration.excluded_directories,
                false
            )
        };
        if (!excluded_directories_result) {
            return std::unexpected{
                config_error(
                    "解析配置文件失败",
                    config_path,
                    excluded_directories_result.error()
                )
            };
        }
        return configuration;
    }

    auto save_configuration(
        std::filesystem::path const& config_path,
        Configuration const& configuration
    ) -> std::expected<void, std::string> {
        auto const parent_path{ config_path.parent_path() };
        if (!parent_path.empty()) {
            std::error_code create_error;
            std::filesystem::create_directories(parent_path, create_error);
            if (create_error) {
                return std::unexpected{
                    config_error("创建配置目录失败", parent_path, create_error.message())
                };
            }
        }

        nlohmann::json config_data;
        config_data["directories"] = configuration.directories |
            std::views::transform(utils::path_to_utf8) |
            std::ranges::to<std::vector<std::string>>();

        config_data["excluded_directories"] = configuration.excluded_directories |
            std::views::transform(utils::path_to_utf8) |
            std::ranges::to<std::vector<std::string>>();

        std::ofstream output{ config_path, std::ios::binary | std::ios::trunc };
        if (!output) {
            return std::unexpected{ config_error("打开配置文件失败", config_path, "无法写入") };
        }
        output << config_data.dump(4) << '\n';
        output.close();
        if (!output) {
            return std::unexpected{ config_error("保存配置文件失败", config_path, "写入错误") };
        }
        return {};
    }

    auto add_directory(
        std::vector<std::filesystem::path>& directories,
        std::filesystem::path directory
    ) -> bool {
        directory = normalize_directory(std::move(directory));
        auto const is_added{
            std::ranges::any_of(directories, [&directory](std::filesystem::path const& added) {
                return utils::paths_equal(added, directory);
            })
        };
        if (is_added) {
            return false;
        }

        directories.emplace_back(std::move(directory));
        return true;
    }

    auto add_directory_input(
        std::vector<std::filesystem::path>& directories,
        std::string_view input
    ) -> std::expected<void, std::string> {
        if (input.empty()) {
            return std::unexpected{ "请输入文件夹路径" };
        }

        std::filesystem::path directory;
        try {
            directory = utils::utf8_to_path(input);
        } catch (std::exception const& error) {
            return std::unexpected{ "文件夹路径无效：" + std::string{ error.what() } };
        }

        std::error_code check_error;
        auto const is_directory{ std::filesystem::is_directory(directory, check_error) };
        if (check_error) {
            return std::unexpected{ "检查文件夹失败：" + check_error.message() };
        }
        if (!is_directory) {
            return std::unexpected{ "路径不是存在的文件夹" };
        }
        if (!add_directory(directories, std::move(directory))) {
            return std::unexpected{ "该文件夹已添加" };
        }
        return {};
    }

    auto monitored_directory_warnings(Configuration const& configuration) -> std::vector<std::string> {
        auto const& directories{ configuration.directories };
        std::vector<std::string> warnings;
        warnings.reserve(directories.size());
        for (auto index{ std::size_t{ 0 } }; index < directories.size(); ++index) {
            warnings.push_back(monitored_directory_warning(directories, index));
        }
        return warnings;
    }

    auto excluded_directory_warnings(Configuration const& configuration) -> std::vector<std::string> {
        std::vector<std::string> warnings;
        warnings.reserve(configuration.excluded_directories.size());
        for (auto const& directory : configuration.excluded_directories) {
            auto const is_monitored{
                std::ranges::any_of(
                    configuration.directories,
                    [&directory](std::filesystem::path const& monitored) {
                        return utils::path_is_within(directory, monitored);
                    }
                )
            };
            warnings.emplace_back(is_monitored ? "" : "不在监控文件夹中，不起作用");
        }
        return warnings;
    }

} // namespace file_monitor::core
