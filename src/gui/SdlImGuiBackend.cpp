#include "SdlImGuiBackend.hpp"

#include <array>
#include <cstdio>
#include <format>
#include <print>
#include <string>

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>

namespace file_monitor::gui {
    namespace {

        // How long an idle window waits for an event before it looks for new
        // file changes.
        constexpr auto IDLE_WAIT_MS{ Sint32{ 100 } };
        // Frames follow an event for this long: the interface reacts to input
        // a frame late, and a tooltip only appears once the pointer has
        // rested.
        constexpr auto ACTIVE_PERIOD_MS{ Uint64{ 1000 } };
        // The frame after the one that shows new content scrolls to it.
        constexpr auto CONTENT_FRAME_COUNT{ 2 };

        auto sdl_error(std::string_view message) -> std::string {
            return std::format("{}: {}", message, SDL_GetError());
        }

        auto log_sdl_error(std::string_view message) -> void {
            std::println(stderr, "{}", sdl_error(message));
        }

        auto configure_opengl_attributes() -> bool {
            return SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0) &&
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE) &&
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3) &&
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0) &&
                SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        }

        auto apply_system_theme() -> void {
            if (SDL_GetSystemTheme() == SDL_SYSTEM_THEME_LIGHT) {
                ImGui::StyleColorsLight();
            } else {
                ImGui::StyleColorsDark();
            }
        }

        auto load_chinese_font(ImGuiIO& io) -> bool {
            constexpr std::array FONT_PATHS{
                "C:/Windows/Fonts/msyh.ttc",
                "C:/Windows/Fonts/simhei.ttf"
            };

            for (auto const* font_path : FONT_PATHS) {
                if (auto* font{ io.Fonts->AddFontFromFileTTF(font_path, 18.0F) }) {
                    io.FontDefault = font;
                    return true;
                }
            }
            return false;
        }

    } // namespace

    SdlImGuiBackend::~SdlImGuiBackend() {
        shutdown();
    }

    auto SdlImGuiBackend::Initialize(std::string_view title, int width, int height)
        -> std::expected<void, std::string> {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            return std::unexpected{ sdl_error("SDL initialization failed") };
        }
        m_sdl_initialized = true;

        if (!configure_opengl_attributes()) {
            return std::unexpected{ sdl_error("OpenGL attribute setup failed") };
        }

        std::string const window_title{ title };
        m_window = SDL_CreateWindow(
            window_title.c_str(),
            width,
            height,
            SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN |
                SDL_WINDOW_HIGH_PIXEL_DENSITY
        );
        if (m_window == nullptr) {
            return std::unexpected{ sdl_error("Window creation failed") };
        }

        if (auto result{ initializeOpenGl() }; !result) {
            return result;
        }
        if (auto result{ initializeImGui() }; !result) {
            return result;
        }

        SDL_SetWindowPosition(m_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        SDL_ShowWindow(m_window);
        m_active_until = SDL_GetTicks() + ACTIVE_PERIOD_MS;
        return {};
    }

    auto SdlImGuiBackend::NextFrame(bool content_changed) -> FrameAction {
        if (content_changed) {
            m_content_frame_count = CONTENT_FRAME_COUNT;
        }

        auto const drawing{
            !isMinimized() && (m_content_frame_count > 0 || SDL_GetTicks() < m_active_until)
        };
        SDL_Event event{};
        auto has_event{ drawing ? SDL_PollEvent(&event) : SDL_WaitEventTimeout(&event, IDLE_WAIT_MS) };
        if (has_event) {
            m_active_until = SDL_GetTicks() + ACTIVE_PERIOD_MS;
        }
        for (; has_event; has_event = SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_SYSTEM_THEME_CHANGED) {
                apply_system_theme();
            }
            if (event.type == SDL_EVENT_QUIT ||
                (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                 event.window.windowID == SDL_GetWindowID(m_window))) {
                return FrameAction::Quit;
            }
        }

        if (isMinimized()) {
            return FrameAction::Skip;
        }
        if (m_content_frame_count > 0) {
            --m_content_frame_count;
            return FrameAction::Render;
        }
        // The cursor of a focused text input blinks.
        if (SDL_GetTicks() < m_active_until || ImGui::GetIO().WantTextInput) {
            return FrameAction::Render;
        }
        return FrameAction::Skip;
    }

    auto SdlImGuiBackend::BeginFrame() -> void {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
    }

    auto SdlImGuiBackend::EndFrame() -> bool {
        ImGui::Render();

        int framebuffer_width{ 0 };
        int framebuffer_height{ 0 };
        if (!SDL_GetWindowSizeInPixels(m_window, &framebuffer_width, &framebuffer_height)) {
            log_sdl_error("Window pixel size query failed");
            return false;
        }

        auto const& clear_color{ ImGui::GetStyleColorVec4(ImGuiCol_WindowBg) };
        glViewport(0, 0, framebuffer_width, framebuffer_height);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (!SDL_GL_SwapWindow(m_window)) {
            log_sdl_error("OpenGL buffer swap failed");
            return false;
        }
        return true;
    }

    auto SdlImGuiBackend::NativeWindow() const -> SDL_Window* {
        return m_window;
    }

    auto SdlImGuiBackend::initializeOpenGl() -> std::expected<void, std::string> {
        m_gl_context = SDL_GL_CreateContext(m_window);
        if (m_gl_context == nullptr) {
            return std::unexpected{ sdl_error("OpenGL context creation failed") };
        }

        if (!SDL_GL_MakeCurrent(m_window, m_gl_context)) {
            return std::unexpected{ sdl_error("OpenGL context activation failed") };
        }

        if (!SDL_GL_SetSwapInterval(1)) {
            log_sdl_error("OpenGL VSync setup failed");
        }
        return {};
    }

    auto SdlImGuiBackend::initializeImGui() -> std::expected<void, std::string> {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        m_imgui_context_created = true;
        apply_system_theme();

        auto& io{ ImGui::GetIO() };
        io.IniFilename = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        if (!load_chinese_font(io)) {
            return std::unexpected{ "Chinese font loading failed" };
        }

        if (!ImGui_ImplSDL3_InitForOpenGL(m_window, m_gl_context)) {
            return std::unexpected{ "ImGui SDL3 backend initialization failed" };
        }
        m_imgui_sdl_initialized = true;

        if (!ImGui_ImplOpenGL3_Init()) {
            return std::unexpected{ "ImGui OpenGL3 backend initialization failed" };
        }
        m_imgui_opengl_initialized = true;
        return {};
    }

    auto SdlImGuiBackend::isMinimized() const -> bool {
        return (SDL_GetWindowFlags(m_window) & SDL_WINDOW_MINIMIZED) != 0;
    }

    auto SdlImGuiBackend::shutdown() -> void {
        if (m_imgui_opengl_initialized) {
            ImGui_ImplOpenGL3_Shutdown();
        }
        if (m_imgui_sdl_initialized) {
            ImGui_ImplSDL3_Shutdown();
        }
        if (m_imgui_context_created) {
            ImGui::DestroyContext();
        }
        if (m_gl_context != nullptr) {
            SDL_GL_DestroyContext(m_gl_context);
        }
        if (m_window != nullptr) {
            SDL_DestroyWindow(m_window);
        }
        if (m_sdl_initialized) {
            SDL_Quit();
        }
    }

} // namespace file_monitor::gui
