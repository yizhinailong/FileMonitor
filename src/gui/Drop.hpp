#pragma once

#include <filesystem>

namespace file_monitor::gui {

    // A file or folder dropped from File Explorer onto the window.
    struct DroppedFile {
        std::filesystem::path path;
        // Where it was dropped, in the coordinates of the window content.
        float x{ 0.0F };
        float y{ 0.0F };
    };

} // namespace file_monitor::gui
