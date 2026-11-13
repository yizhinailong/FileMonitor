module;

#include <filesystem>
#include <fstream>
#include <optional>
#include <system_error>

export module file_monitor.core:window_layout;

import nlohmann.json;

namespace file_monitor::core {
    namespace {

        auto read_int(nlohmann::json const& data, char const* key, int& value) -> bool {
            auto const member{ data.find(key) };
            if (member == data.end() || !member->is_number_integer()) {
                return false;
            }
            value = member->get<int>();
            return true;
        }

    } // namespace

    // The size and position of the desktop window, so that it reopens where
    // the user left it.
    export struct WindowLayout {
        int x{ 0 };
        int y{ 0 };
        int width{ 0 };
        int height{ 0 };
        bool maximized{ false };
    };

    // The layout stored in `path`, or nothing when there is none or it cannot
    // be read.
    export auto load_window_layout(std::filesystem::path const& path) -> std::optional<WindowLayout> {
        std::error_code exists_error;
        if (!std::filesystem::exists(path, exists_error) || exists_error) {
            return std::nullopt;
        }

        std::ifstream input{ path, std::ios::binary };
        if (!input) {
            return std::nullopt;
        }
        auto const data = nlohmann::json::parse(input, nullptr, false);
        if (!data.is_object()) {
            return std::nullopt;
        }

        WindowLayout layout;
        if (!read_int(data, "x", layout.x) ||
            !read_int(data, "y", layout.y) ||
            !read_int(data, "width", layout.width) ||
            !read_int(data, "height", layout.height) ||
            layout.width <= 0 ||
            layout.height <= 0) {
            return std::nullopt;
        }
        if (auto const member{ data.find("maximized") }; member != data.end() && member->is_boolean()) {
            layout.maximized = member->get<bool>();
        }
        return layout;
    }

    export auto save_window_layout(std::filesystem::path const& path, WindowLayout const& layout) -> bool {
        auto const parent_path{ path.parent_path() };
        if (!parent_path.empty()) {
            std::error_code create_error;
            std::filesystem::create_directories(parent_path, create_error);
            if (create_error) {
                return false;
            }
        }

        nlohmann::json data;
        data["x"] = layout.x;
        data["y"] = layout.y;
        data["width"] = layout.width;
        data["height"] = layout.height;
        data["maximized"] = layout.maximized;

        std::ofstream output{ path, std::ios::binary | std::ios::trunc };
        if (!output) {
            return false;
        }
        output << data.dump(4) << '\n';
        output.close();
        return static_cast<bool>(output);
    }

} // namespace file_monitor::core
