#include "LogWindow.hpp"

#include <algorithm>
#include <chrono>
#include <format>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <imgui.h>

namespace file_monitor::ui::windows {
    namespace {

        auto path_to_utf8(std::filesystem::path const& path) -> std::string {
            auto const utf8_path{ path.generic_u8string() };
            return { reinterpret_cast<char const*>(utf8_path.data()), utf8_path.size() };
        }

        auto format_file_info(
            std::filesystem::directory_entry const& entry,
            std::filesystem::path const&            root
        ) -> std::string {
            auto const relative_path{ entry.path().lexically_relative(root) };

            std::error_code size_error;
            auto const      size{ entry.file_size(size_error) };
            auto const      size_text{
                size_error ? std::string{ "unknown size" } : std::format("{} bytes", size)
            };

            std::error_code time_error;
            auto const      file_time{ entry.last_write_time(time_error) };
            auto            time_text{ std::string{ "unknown modification time" } };
            if (!time_error) {
                auto const system_time{ std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    file_time - decltype(file_time)::clock::now() + std::chrono::system_clock::now()
                ) };
                time_text = std::format(
                    "{:%Y-%m-%d %H:%M:%S}",
                    std::chrono::floor<std::chrono::seconds>(system_time)
                );
            }

            return std::format(
                "{} | {} | {}",
                path_to_utf8(relative_path),
                size_text,
                time_text
            );
        }

    } // namespace

    LogWindow::LogWindow()
        : Window{ "LogWindow" } {
    }

    auto LogWindow::SetDirectory(std::filesystem::path const& directory) -> void {
        std::vector<std::string> files;
        std::error_code          iteration_error;
        auto                     iterator{
            std::filesystem::recursive_directory_iterator{
                                                          directory,
                                                          std::filesystem::directory_options::skip_permission_denied,
                                                          iteration_error }
        };
        auto const end{ std::filesystem::recursive_directory_iterator{} };

        while (!iteration_error && iterator != end) {
            std::error_code type_error;
            if (iterator->is_regular_file(type_error) && !type_error) {
                files.emplace_back(format_file_info(*iterator, directory));
            }
            iterator.increment(iteration_error);
        }

        std::ranges::sort(files);
        m_file_list.SetItems(std::move(files));

        auto const directory_text{ path_to_utf8(directory) };
        if (iteration_error) {
            m_summary_text.SetContent(std::format("Unable to read all files in {}: {}", directory_text, iteration_error.message()));
        } else {
            m_summary_text.SetContent(std::format("{} file(s) in {}", m_file_list.ItemCount(), directory_text));
        }
    }

    auto LogWindow::renderContent() -> void {
        m_summary_text.Render();
        auto const available{ ImGui::GetContentRegionAvail() };
        m_file_list.Render(available.x, available.y);
    }

} // namespace file_monitor::ui::windows
