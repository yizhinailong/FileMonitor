#pragma once

#include <expected>
#include <string>
#include <string_view>

struct SDL_GLContextState;
struct SDL_Window;

namespace file_monitor::ui {

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
        auto ProcessEvents() -> bool;
        auto BeginFrame() -> bool;
        auto EndFrame() -> bool;
        [[nodiscard]]
        auto NativeWindow() const -> SDL_Window*;

    private:
        auto initializeOpenGl() -> std::expected<void, std::string>;
        auto initializeImGui() -> std::expected<void, std::string>;
        auto shutdown() -> void;

        SDL_Window* m_window{ nullptr };
        SDL_GLContextState* m_gl_context{ nullptr };
        bool m_sdl_initialized{ false };
        bool m_imgui_context_created{ false };
        bool m_imgui_sdl_initialized{ false };
        bool m_imgui_opengl_initialized{ false };
    };

} // namespace file_monitor::ui
