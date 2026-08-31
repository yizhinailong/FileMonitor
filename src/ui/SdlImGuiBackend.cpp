#include "SdlImGuiBackend.hpp"

#include <array>
#include <cstdio>
#include <format>
#include <print>
#include <string>

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>

namespace file_monitor::ui {
    namespace {

        constexpr auto SHADER_FORMATS = SDL_GPU_SHADERFORMAT_SPIRV |
                                        SDL_GPU_SHADERFORMAT_DXIL |
                                        SDL_GPU_SHADERFORMAT_MSL |
                                        SDL_GPU_SHADERFORMAT_METALLIB;

        auto sdl_error(std::string_view message) -> std::string {
            return std::format("{}: {}", message, SDL_GetError());
        }

        auto log_sdl_error(std::string_view message) -> void {
            std::println(stderr, "{}", sdl_error(message));
        }

        auto apply_system_theme() -> void {
            if (SDL_GetSystemTheme() == SDL_SYSTEM_THEME_LIGHT) {
                ImGui::StyleColorsLight();
            } else {
                ImGui::StyleColorsDark();
            }
        }

        auto load_chinese_font(ImGuiIO& io) -> bool {
#if defined(_WIN32)
            constexpr std::array FONT_PATHS{
                "C:/Windows/Fonts/msyh.ttc",
                "C:/Windows/Fonts/simhei.ttf"
            };
#elif defined(__APPLE__)
            constexpr std::array FONT_PATHS{
                "/System/Library/Fonts/PingFang.ttc",
                "/System/Library/Fonts/STHeiti Light.ttc"
            };
#else
            constexpr std::array FONT_PATHS{
                "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
                "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc"
            };
#endif

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
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
            return std::unexpected{ sdl_error("SDL initialization failed") };
        }
        m_sdl_initialized = true;

        std::string const window_title{ title };
        m_window = SDL_CreateWindow(
            window_title.c_str(),
            width,
            height,
            SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY
        );
        if (m_window == nullptr) {
            return std::unexpected{ sdl_error("Window creation failed") };
        }

        if (auto result{ initializeGpu() }; !result) {
            return result;
        }
        if (auto result{ initializeImGui() }; !result) {
            return result;
        }

        SDL_SetWindowPosition(m_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        SDL_ShowWindow(m_window);
        return {};
    }

    auto SdlImGuiBackend::ProcessEvents() -> bool {
        SDL_Event event{};
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_SYSTEM_THEME_CHANGED) {
                apply_system_theme();
            }
            if (event.type == SDL_EVENT_QUIT ||
                (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                 event.window.windowID == SDL_GetWindowID(m_window))) {
                return false;
            }
        }

        return true;
    }

    auto SdlImGuiBackend::BeginFrame() -> bool {
        if ((SDL_GetWindowFlags(m_window) & SDL_WINDOW_MINIMIZED) != 0) {
            SDL_Delay(10);
            return false;
        }

        ImGui_ImplSDLGPU3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        return true;
    }

    auto SdlImGuiBackend::EndFrame() -> bool {
        ImGui::Render();

        auto* draw_data{ ImGui::GetDrawData() };
        auto* command_buffer{ SDL_AcquireGPUCommandBuffer(m_gpu_device) };
        if (command_buffer == nullptr) {
            log_sdl_error("GPU command buffer acquisition failed");
            return false;
        }

        SDL_GPUTexture* swapchain_texture{ nullptr };
        if (!SDL_WaitAndAcquireGPUSwapchainTexture(
                command_buffer,
                m_window,
                &swapchain_texture,
                nullptr,
                nullptr
            )) {
            SDL_CancelGPUCommandBuffer(command_buffer);
            log_sdl_error("GPU swapchain texture acquisition failed");
            return false;
        }

        bool const minimized{ draw_data->DisplaySize.x <= 0.0F || draw_data->DisplaySize.y <= 0.0F };
        if (swapchain_texture != nullptr && !minimized) {
            ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, command_buffer);

            auto const&            clear_color{ ImGui::GetStyleColorVec4(ImGuiCol_WindowBg) };
            SDL_GPUColorTargetInfo target_info{};
            target_info.texture     = swapchain_texture;
            target_info.clear_color = { clear_color.x, clear_color.y, clear_color.z, clear_color.w };
            target_info.load_op     = SDL_GPU_LOADOP_CLEAR;
            target_info.store_op    = SDL_GPU_STOREOP_STORE;

            auto* render_pass{ SDL_BeginGPURenderPass(command_buffer, &target_info, 1, nullptr) };
            if (render_pass == nullptr) {
                SDL_CancelGPUCommandBuffer(command_buffer);
                log_sdl_error("GPU render pass creation failed");
                return false;
            }

            ImGui_ImplSDLGPU3_RenderDrawData(draw_data, command_buffer, render_pass);
            SDL_EndGPURenderPass(render_pass);
        }

        if (!SDL_SubmitGPUCommandBuffer(command_buffer)) {
            log_sdl_error("GPU frame submission failed");
            return false;
        }
        return true;
    }

    auto SdlImGuiBackend::NativeWindow() const -> SDL_Window* {
        return m_window;
    }

    auto SdlImGuiBackend::initializeGpu() -> std::expected<void, std::string> {
        m_gpu_device = SDL_CreateGPUDevice(SHADER_FORMATS, false, nullptr);
        if (m_gpu_device == nullptr) {
            return std::unexpected{ sdl_error("GPU device creation failed") };
        }

        if (!SDL_ClaimWindowForGPUDevice(m_gpu_device, m_window)) {
            return std::unexpected{ sdl_error("GPU window claim failed") };
        }
        m_window_claimed = true;

        if (!SDL_SetGPUSwapchainParameters(
                m_gpu_device,
                m_window,
                SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                SDL_GPU_PRESENTMODE_VSYNC
            )) {
            return std::unexpected{ sdl_error("GPU swapchain setup failed") };
        }
        return {};
    }

    auto SdlImGuiBackend::initializeImGui() -> std::expected<void, std::string> {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        m_imgui_context_created = true;
        apply_system_theme();

        auto& io{ ImGui::GetIO() };
        io.IniFilename  = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        if (!load_chinese_font(io)) {
            return std::unexpected{ "Chinese font loading failed" };
        }

        if (!ImGui_ImplSDL3_InitForSDLGPU(m_window)) {
            return std::unexpected{ "ImGui SDL3 backend initialization failed" };
        }
        m_imgui_sdl_initialized = true;

        ImGui_ImplSDLGPU3_InitInfo init_info{};
        init_info.Device               = m_gpu_device;
        init_info.ColorTargetFormat    = SDL_GetGPUSwapchainTextureFormat(m_gpu_device, m_window);
        init_info.MSAASamples          = SDL_GPU_SAMPLECOUNT_1;
        init_info.SwapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;
        init_info.PresentMode          = SDL_GPU_PRESENTMODE_VSYNC;
        if (!ImGui_ImplSDLGPU3_Init(&init_info)) {
            return std::unexpected{
                sdl_error("ImGui SDL_GPU backend initialization failed")
            };
        }
        m_imgui_gpu_initialized = true;
        return {};
    }

    auto SdlImGuiBackend::shutdown() -> void {
        if (m_gpu_device != nullptr) {
            SDL_WaitForGPUIdle(m_gpu_device);
        }
        if (m_imgui_gpu_initialized) {
            ImGui_ImplSDLGPU3_Shutdown();
        }
        if (m_imgui_sdl_initialized) {
            ImGui_ImplSDL3_Shutdown();
        }
        if (m_imgui_context_created) {
            ImGui::DestroyContext();
        }
        if (m_window_claimed) {
            SDL_ReleaseWindowFromGPUDevice(m_gpu_device, m_window);
        }
        if (m_gpu_device != nullptr) {
            SDL_DestroyGPUDevice(m_gpu_device);
        }
        if (m_window != nullptr) {
            SDL_DestroyWindow(m_window);
        }
        if (m_sdl_initialized) {
            SDL_Quit();
        }
    }

} // namespace file_monitor::ui
