#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "Drop.hpp"

import file_monitor.core;

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
        SdlImGuiBackend& operator=(SdlImGuiBackend const&) = delete;
        SdlImGuiBackend(SdlImGuiBackend&&) = delete;
        SdlImGuiBackend& operator=(SdlImGuiBackend&&) = delete;

        std::expected<void, std::string> Initialize(std::string_view title, int width, int height);
        // Handles the pending events and tells what to do with the next
        // frame. Frames are drawn for a moment after an event or after
        // `content_changed`; without either the call waits for an event for a
        // short time and skips the frame, so an idle window costs nothing.
        FrameAction NextFrame(bool content_changed);
        // The files and folders dropped onto the window since the last call.
        [[nodiscard]] std::vector<DroppedFile> TakeDroppedFiles();
        void BeginFrame();
        bool EndFrame();
        [[nodiscard]] SDL_Window* NativeWindow() const;

    private:
        std::expected<void, std::string> initializeOpenGl();
        std::expected<void, std::string> initializeImGui();
        [[nodiscard]] bool isMinimized() const;
        // Puts the window where it was when it closed last time, or in the
        // middle of the primary display.
        void applySavedLayout();
        void saveLayout();
        void shutdown();

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
        // The size and position of the window while it is neither minimized
        // nor maximized, which is what a saved layout restores.
        core::WindowLayout m_normal_layout;
        bool m_sdl_initialized{ false };
        bool m_imgui_context_created{ false };
        bool m_imgui_sdl_initialized{ false };
        bool m_imgui_opengl_initialized{ false };
    };

} // namespace file_monitor::gui
