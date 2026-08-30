#include "SdlImGuiBackend.hpp"

#include <array>
#include <cstdio>
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

        auto log_sdl_error(char const* message) -> void {
            std::fprintf(stderr, "%s: %s\n", message, SDL_GetError());
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

    auto SdlImGuiBackend::Initialize(std::string_view title, int width, int height) -> bool {
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
            log_sdl_error("SDL initialization failed");
            return false;
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
            log_sdl_error("Window creation failed");
            return false;
        }

        if (!initializeGpu() || !initializeImGui()) {
            return false;
        }

        SDL_SetWindowPosition(m_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        SDL_ShowWindow(m_window);
        return true;
    }

    auto SdlImGuiBackend::ProcessEvents() -> bool {
        SDL_Event event{};
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
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

    auto SdlImGuiBackend::initializeGpu() -> bool {
        m_gpu_device = SDL_CreateGPUDevice(SHADER_FORMATS, false, nullptr);
        if (m_gpu_device == nullptr) {
            log_sdl_error("GPU device creation failed");
            return false;
        }

        if (!SDL_ClaimWindowForGPUDevice(m_gpu_device, m_window)) {
            log_sdl_error("GPU window claim failed");
            return false;
        }
        m_window_claimed = true;

        if (!SDL_SetGPUSwapchainParameters(
                m_gpu_device,
                m_window,
                SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                SDL_GPU_PRESENTMODE_VSYNC
            )) {
            log_sdl_error("GPU swapchain setup failed");
            return false;
        }
        return true;
    }

    auto SdlImGuiBackend::initializeImGui() -> bool {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        m_imgui_context_created = true;

        auto& io{ ImGui::GetIO() };
        io.IniFilename  = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        if (!load_chinese_font(io)) {
            std::fprintf(stderr, "Chinese font loading failed\n");
            return false;
        }

        if (!ImGui_ImplSDL3_InitForSDLGPU(m_window)) {
            std::fprintf(stderr, "ImGui SDL3 backend initialization failed\n");
            return false;
        }
        m_imgui_sdl_initialized = true;

        ImGui_ImplSDLGPU3_InitInfo init_info{};
        init_info.Device               = m_gpu_device;
        init_info.ColorTargetFormat    = SDL_GetGPUSwapchainTextureFormat(m_gpu_device, m_window);
        init_info.MSAASamples          = SDL_GPU_SAMPLECOUNT_1;
        init_info.SwapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;
        init_info.PresentMode          = SDL_GPU_PRESENTMODE_VSYNC;
        if (!ImGui_ImplSDLGPU3_Init(&init_info)) {
            log_sdl_error("ImGui SDL_GPU backend initialization failed");
            return false;
        }
        m_imgui_gpu_initialized = true;
        return true;
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
