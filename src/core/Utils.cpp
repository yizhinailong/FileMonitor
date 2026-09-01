module;

#include <filesystem>
#include <string>
#include <string_view>

module core;

namespace file_monitor::core::utils {

    auto path_to_utf8(std::filesystem::path const& path) -> std::string {
        auto const utf8_path{ path.u8string() };
        return { reinterpret_cast<char const*>(utf8_path.data()), utf8_path.size() };
    }

    auto utf8_to_path(std::string_view path) -> std::filesystem::path {
        auto const* begin{ reinterpret_cast<char8_t const*>(path.data()) };
        return std::filesystem::path{
            std::u8string{ begin, begin + path.size() }
        };
    }

} // namespace file_monitor::core::utils
