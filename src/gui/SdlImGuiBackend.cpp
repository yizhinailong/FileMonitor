#include "SdlImGuiBackend.hpp"

#include <cstdio>
#include <format>
#include <print>
#include <string>
#include <utility>

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>

#include "Style.hpp"
#include "core/Utils.hpp"
#include "core/WindowLayout.hpp"

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
            apply_theme(SDL_GetSystemTheme() == SDL_SYSTEM_THEME_LIGHT ? Theme::Light : Theme::Dark);
        }

        auto window_layout_path() -> std::filesystem::path {
            return core::utils::data_directory() / "window-layout.json";
        }

        // The scale Windows reports for the display the window is on.
        auto display_scale(SDL_Window* window) -> float {
            auto const scale{ SDL_GetWindowDisplayScale(window) };
            return scale > 0.0F ? scale : 1.0F;
        }

        // Whether the window would at least partly be on one of the displays;
        // a layout saved on a display that is gone would leave it out of
        // reach otherwise.
        auto is_on_a_display(core::WindowLayout const& layout) -> bool {
            auto const window{
                SDL_Rect{ layout.x, layout.y, layout.width, layout.height }
            };
            auto display_count{ 0 };
            auto* const displays{ SDL_GetDisplays(&display_count) };
            auto visible{ false };
            for (auto index{ 0 }; index < display_count && !visible; ++index) {
                SDL_Rect bounds{};
                if (SDL_GetDisplayUsableBounds(displays[index], &bounds)) {
                    visible = window.x < bounds.x + bounds.w &&
                        bounds.x < window.x + window.w &&
                        window.y < bounds.y + bounds.h &&
                        bounds.y < window.y + window.h;
                }
            }
            SDL_free(displays);
            return visible;
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

        auto const scale{ SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay()) };
        auto const window_scale{ scale > 0.0F ? scale : 1.0F };
        std::string const window_title{ title };
        m_window = SDL_CreateWindow(
            window_title.c_str(),
            static_cast<int>(static_cast<float>(width) * window_scale),
            static_cast<int>(static_cast<float>(height) * window_scale),
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

        applySavedLayout();
        SDL_ShowWindow(m_window);
        m_active_until = SDL_GetTicks() + ACTIVE_PERIOD_MS;
        return {};
    }

    auto SdlImGuiBackend::applySavedLayout() -> void {
        auto const layout{ core::load_window_layout(window_layout_path()) };
        if (!layout) {
            SDL_SetWindowPosition(m_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
            return;
        }
        SDL_SetWindowSize(m_window, layout->width, layout->height);
        if (is_on_a_display(*layout)) {
            SDL_SetWindowPosition(m_window, layout->x, layout->y);
        } else {
            SDL_SetWindowPosition(m_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        }
        if (layout->maximized) {
            SDL_MaximizeWindow(m_window);
        }
    }

    auto SdlImGuiBackend::saveLayout() -> void {
        core::WindowLayout layout{ m_normal_layout };
        layout.maximized = (SDL_GetWindowFlags(m_window) & SDL_WINDOW_MAXIMIZED) != 0;
        core::save_window_layout(window_layout_path(), layout);
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
            if (event.type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED) {
                set_scale(display_scale(m_window));
            }
            // A drop from File Explorer lists its files before it reports
            // where they were dropped.
            if (event.type == SDL_EVENT_DROP_FILE && event.drop.data != nullptr) {
                m_drop_files.emplace_back(core::utils::utf8_to_path(event.drop.data));
            } else if (event.type == SDL_EVENT_DROP_POSITION) {
                m_drop_x = event.drop.x;
                m_drop_y = event.drop.y;
            } else if (event.type == SDL_EVENT_DROP_COMPLETE) {
                for (auto& path : m_drop_files) {
                    m_dropped_files.push_back({ std::move(path), m_drop_x, m_drop_y });
                }
                m_drop_files.clear();
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
        // The size to restore the window to, should it close while maximized
        // or minimized.
        if ((SDL_GetWindowFlags(m_window) & SDL_WINDOW_MAXIMIZED) == 0) {
            SDL_GetWindowPosition(m_window, &m_normal_layout.x, &m_normal_layout.y);
            SDL_GetWindowSize(m_window, &m_normal_layout.width, &m_normal_layout.height);
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

    auto SdlImGuiBackend::TakeDroppedFiles() -> std::vector<DroppedFile> {
        return std::exchange(m_dropped_files, {});
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

        set_scale(display_scale(m_window));
        auto& io{ ImGui::GetIO() };
        io.IniFilename = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        if (!load_font()) {
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
            saveLayout();
            SDL_DestroyWindow(m_window);
        }
        if (m_sdl_initialized) {
            SDL_Quit();
        }
    }

} // namespace file_monitor::gui
