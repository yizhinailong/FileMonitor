module;

#include <algorithm>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#ifndef NOMINMAX
    #define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

export module file_monitor.tui:path_completion;

import file_monitor.core;

namespace file_monitor::tui {

    export struct PathCompletion {
        // The input with its last segment completed as far as the matching
        // directories agree; it ends with a separator when one directory
        // matches.
        std::string text;
        // The names of the matching directories, sorted, when more than one
        // matches.
        std::vector<std::string> candidates;
        bool matched{ false };
    };

    // Completes the last segment of a directory path with the names of the
    // subdirectories that start with it, ignoring case like Windows does. A
    // relative path starts from the current working directory.
    export auto complete_directory_path(std::string_view input) -> PathCompletion;

    namespace {

        // Keeps a Tab press quick in a directory with very many entries.
        constexpr auto MAX_SCANNED_ENTRIES{ 5000 };

        auto compare_ignoring_case(std::wstring_view left, std::wstring_view right) -> int {
            return CompareStringOrdinal(
                left.data(),
                static_cast<int>(left.size()),
                right.data(),
                static_cast<int>(right.size()),
                TRUE
            );
        }

        auto starts_with_ignoring_case(std::wstring_view name, std::wstring_view prefix) -> bool {
            return name.size() >= prefix.size() &&
                compare_ignoring_case(name.substr(0, prefix.size()), prefix) == CSTR_EQUAL;
        }

        // The length of the start that all names share, ignoring case.
        auto shared_prefix_length(std::vector<std::wstring> const& names) -> std::size_t {
            std::wstring_view const first{ names.front() };
            auto length{ first.size() };
            for (std::wstring_view const name : names | std::views::drop(1)) {
                std::size_t shared{ 0 };
                while (shared < length &&
                       shared < name.size() &&
                       compare_ignoring_case(first.substr(shared, 1), name.substr(shared, 1)) == CSTR_EQUAL) {
                    ++shared;
                }
                length = shared;
            }
            // Do not split a surrogate pair.
            if (length > 0 && IS_HIGH_SURROGATE(first[length - 1])) {
                --length;
            }
            return length;
        }

        auto to_utf8(std::wstring const& name) -> std::string {
            return core::utils::path_to_utf8(std::filesystem::path{ name });
        }

        auto matching_directories(std::filesystem::path const& directory, std::wstring_view prefix)
            -> std::vector<std::wstring> {
            std::vector<std::wstring> names;
            std::error_code error;
            std::filesystem::directory_iterator entry{
                directory,
                std::filesystem::directory_options::skip_permission_denied,
                error
            };
            for (auto scanned{ 0 };
                 !error && entry != std::filesystem::directory_iterator{} && scanned < MAX_SCANNED_ENTRIES;
                 ++scanned) {
                std::error_code type_error;
                auto name{ entry->path().filename().native() };
                if (entry->is_directory(type_error) && starts_with_ignoring_case(name, prefix)) {
                    names.push_back(std::move(name));
                }
                entry.increment(error);
            }
            std::ranges::sort(names, [](std::wstring const& left, std::wstring const& right) {
                return compare_ignoring_case(left, right) == CSTR_LESS_THAN;
            });
            return names;
        }

        auto complete_valid_directory_path(std::string_view input) -> PathCompletion {
            PathCompletion completion{ .text = std::string{ input } };

            std::string base{ input };
            // "C:" alone names the working directory of drive C; complete the
            // drive root instead.
            if (base.size() == 2 && base[1] == ':') {
                base += '\\';
            }
            auto const separator{ base.find_last_of("\\/") };
            auto const prefix{
                core::utils::utf8_to_path(separator == std::string::npos ? base : base.substr(separator + 1))
            };
            base.resize(separator == std::string::npos ? 0 : separator + 1);

            std::error_code error;
            auto const directory{
                base.empty() ? std::filesystem::current_path(error) : core::utils::utf8_to_path(base)
            };
            if (error) {
                return completion;
            }

            auto const names{ matching_directories(directory, prefix.native()) };
            if (names.empty()) {
                return completion;
            }
            completion.matched = true;
            if (names.size() == 1) {
                completion.text = base + to_utf8(names.front()) + "\\";
                return completion;
            }

            auto const shared_length{ shared_prefix_length(names) };
            if (shared_length > prefix.native().size()) {
                completion.text = base + to_utf8(names.front().substr(0, shared_length));
            }
            completion.candidates = names |
                std::views::transform(to_utf8) |
                std::ranges::to<std::vector<std::string>>();
            return completion;
        }

    } // namespace

    auto complete_directory_path(std::string_view input) -> PathCompletion {
        try {
            return complete_valid_directory_path(input);
        } catch (std::exception const&) {
            // The input is not a valid path.
            return PathCompletion{ .text = std::string{ input } };
        }
    }

} // namespace file_monitor::tui
