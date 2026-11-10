#include "Utils.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>

#ifndef NOMINMAX
    #define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace file_monitor::core::utils {
    namespace {

        constexpr auto MAX_EXTENDED_PATH_LENGTH{ std::size_t{ 32'767 } };

        auto path_elements_equal(
            std::filesystem::path const& left,
            std::filesystem::path const& right
        ) -> bool {
            auto const& left_text{ left.native() };
            auto const& right_text{ right.native() };
            return CompareStringOrdinal(
                       left_text.data(),
                       static_cast<int>(left_text.size()),
                       right_text.data(),
                       static_cast<int>(right_text.size()),
                       TRUE
                   ) == CSTR_EQUAL;
        }

    } // namespace

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

    auto executable_directory() -> std::filesystem::path {
        std::wstring module_path(MAX_PATH, L'\0');
        while (true) {
            auto const length{
                GetModuleFileNameW(
                    nullptr,
                    module_path.data(),
                    static_cast<DWORD>(module_path.size())
                )
            };
            if (length == 0) {
                return {};
            }
            if (length < module_path.size()) {
                module_path.resize(length);
                return std::filesystem::path{ std::move(module_path) }.parent_path();
            }
            if (module_path.size() >= MAX_EXTENDED_PATH_LENGTH) {
                return {};
            }
            module_path.resize(std::min(module_path.size() * 2, MAX_EXTENDED_PATH_LENGTH));
        }
    }

    // Windows file systems treat paths case-insensitively, so path elements are
    // compared the same way; a trailing separator on directory is ignored.
    auto path_is_within(
        std::filesystem::path const& path,
        std::filesystem::path const& directory
    ) -> bool {
        if (directory.empty()) {
            return false;
        }

        auto path_element{ path.begin() };
        auto const path_end{ path.end() };
        for (auto const& directory_element : directory) {
            if (directory_element.empty()) {
                continue;
            }
            if (path_element == path_end ||
                !path_elements_equal(*path_element, directory_element)) {
                return false;
            }
            ++path_element;
        }
        return true;
    }

} // namespace file_monitor::core::utils
