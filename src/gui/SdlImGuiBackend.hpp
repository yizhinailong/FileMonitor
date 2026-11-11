#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "Drop.hpp"

struct SDL_GLContextState;
struct SDL_Window;

namespace file_monitor::gui {

    enum class FrameAction {
        Quit,
        Skip,
        Render
    };

    class SdlImGuiBackend final {
    public:
        SdlImGuiBackend() = default;
        ~SdlImGuiBackend();

        SdlImGuiBackend(SdlImGuiBackend const&) = delete;
        auto operator=(SdlImGuiBackend const&) -> SdlImGuiBackend& = delete;
        SdlImGuiBackend(SdlImGuiBackend&&) = delete;
        auto operator=(SdlImGuiBackend&&) -> SdlImGuiBackend& = delete;

        auto Initialize(std::string_view title, int width, int height)
            -> std::expected<void, std::string>;
        // Handles the pending events and tells what to do with the next
        // frame. Frames are drawn for a moment after an event or after
        // `content_changed`; without either the call waits for an event for a
        // short time and skips the frame, so an idle window costs nothing.
        auto NextFrame(bool content_changed) -> FrameAction;
        // The files and folders dropped onto the window since the last call.
        [[nodiscard]]
        auto TakeDroppedFiles() -> std::vector<DroppedFile>;
        auto BeginFrame() -> void;
        auto EndFrame() -> bool;
        [[nodiscard]]
        auto NativeWindow() const -> SDL_Window*;

    private:
        auto initializeOpenGl() -> std::expected<void, std::string>;
        auto initializeImGui() -> std::expected<void, std::string>;
        [[nodiscard]]
        auto isMinimized() const -> bool;
        auto shutdown() -> void;

        SDL_Window* m_window{ nullptr };
        SDL_GLContextState* m_gl_context{ nullptr };
        // SDL_GetTicks() until which frames follow the last event.
        std::uint64_t m_active_until{ 0 };
        // Frames still to draw for changed content.
        int m_content_frame_count{ 0 };
        // The files of a drop in progress, which SDL reports before it
        // reports where the drop ended.
        std::vector<std::filesystem::path> m_drop_files;
        std::vector<DroppedFile> m_dropped_files;
        float m_drop_x{ 0.0F };
        float m_drop_y{ 0.0F };
        bool m_sdl_initialized{ false };
        bool m_imgui_context_created{ false };
        bool m_imgui_sdl_initialized{ false };
        bool m_imgui_opengl_initialized{ false };
    };

} // namespace file_monitor::gui
