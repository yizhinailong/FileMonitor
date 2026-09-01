#include "Configuration.hpp"

#include <algorithm>
#include <format>
#include <fstream>
#include <ranges>
#include <string_view>
#include <system_error>
#include <utility>

#include <nlohmann/json.hpp>

#include "Utils.hpp"

namespace file_monitor::core {
    namespace {

        auto config_error(
            std::string_view             operation,
            std::filesystem::path const& config_path,
            std::string_view             detail
        ) -> std::string {
            return std::format(
                "{} {}：{}",
                operation,
                utils::path_to_utf8(config_path),
                detail
            );
        }

        auto load_directories(
            nlohmann::json const&               config_data,
            std::string_view                    key,
            std::vector<std::filesystem::path>& directories,
            bool                                required
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

                auto directory{ utils::utf8_to_path(directory_text).lexically_normal() };
                if (!std::ranges::contains(directories, directory)) {
                    directories.emplace_back(std::move(directory));
                }
            }
            return {};
        }

    } // namespace

    auto load_configuration(std::filesystem::path const& config_path)
        -> std::expected<Configuration, std::string> {
        std::error_code exists_error;
        auto const      config_exists{ std::filesystem::exists(config_path, exists_error) };
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
        auto const    directories_result{
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
        Configuration const&         configuration
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
        config_data["directories"]          = configuration.directories |
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

} // namespace file_monitor::core
