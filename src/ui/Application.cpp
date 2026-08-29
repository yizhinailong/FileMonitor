#include "Application.hpp"

#include <algorithm>
#include <cstdio>

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>

namespace file_monitor::ui {
    namespace {

        constexpr int initial_width  = 960;
        constexpr int initial_height = 640;

        struct ApplicationState {
            bool dark_mode = true;
        };

        void apply_theme(bool dark_mode) {
            if (dark_mode) {
                ImGui::StyleColorsDark();
            } else {
                ImGui::StyleColorsLight();
            }

            auto& style             = ImGui::GetStyle();
            style.WindowRounding    = 0.0F;
            style.ChildRounding     = 0.0F;
            style.FrameRounding     = 6.0F;
            style.PopupRounding     = 6.0F;
            style.ScrollbarRounding = 6.0F;
            style.GrabRounding      = 6.0F;
            style.WindowBorderSize  = 0.0F;
            style.FrameBorderSize   = 0.0F;
            style.WindowPadding     = { 24.0F, 20.0F };
            style.FramePadding      = { 12.0F, 8.0F };
            style.ItemSpacing       = { 10.0F, 10.0F };
        }

        void center_text(const char* text, float vertical_offset = 0.0F) {
            const auto available = ImGui::GetContentRegionAvail();
            const auto text_size = ImGui::CalcTextSize(text);
            ImGui::SetCursorPos({
                ImGui::GetCursorPosX() + std::max(0.0F, (available.x - text_size.x) * 0.5F),
                ImGui::GetCursorPosY() + std::max(0.0F, (available.y - text_size.y) * 0.5F) + vertical_offset,
            });
            ImGui::TextUnformatted(text);
        }

        void render_workspace(ApplicationState& state) {
            const auto* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);

            constexpr auto window_flags = ImGuiWindowFlags_NoDecoration |
                                          ImGuiWindowFlags_NoMove |
                                          ImGuiWindowFlags_NoSavedSettings |
                                          ImGuiWindowFlags_NoBringToFrontOnFocus;

            ImGui::Begin("FileMonitorWorkspace", nullptr, window_flags);

            const auto available    = ImGui::GetContentRegionAvail();
            const auto column_width = std::max(1.0F, available.x * 0.5F);

            ImGui::BeginChild("PrimaryPane", { column_width, available.y }, false);
            ImGui::SetWindowFontScale(2.4F);
            center_text("FileMonitor");
            ImGui::SetWindowFontScale(1.0F);
            ImGui::EndChild();

            ImGui::SameLine(0.0F, 0.0F);

            ImGui::PushStyleColor(
                ImGuiCol_ChildBg,
                state.dark_mode ? ImVec4{ 0.18F, 0.18F, 0.18F, 1.0F } : ImVec4{ 0.87F, 0.87F, 0.87F, 1.0F }
            );
            ImGui::BeginChild("SettingsPane", { 0.0F, available.y }, false);

            const auto control_size = ImGui::CalcTextSize("Dark mode");
            ImGui::SetCursorPos({
                std::max(0.0F, (ImGui::GetContentRegionAvail().x - control_size.x - 28.0F) * 0.5F),
                std::max(0.0F, ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeight() - 16.0F),
            });
            if (ImGui::Checkbox("Dark mode", &state.dark_mode)) {
                apply_theme(state.dark_mode);
            }

            ImGui::EndChild();
            ImGui::PopStyleColor();
            ImGui::End();
        }

        bool render_frame(SDL_GPUDevice* gpu_device, SDL_Window* window) {
            ImGui::Render();

            auto* draw_data = ImGui::GetDrawData();
            auto* command_buffer = SDL_AcquireGPUCommandBuffer(gpu_device);
            if (command_buffer == nullptr) {
                return false;
            }

            SDL_GPUTexture* swapchain_texture = nullptr;
            if (!SDL_WaitAndAcquireGPUSwapchainTexture(
                    command_buffer,
                    window,
                    &swapchain_texture,
                    nullptr,
                    nullptr
                )) {
                SDL_CancelGPUCommandBuffer(command_buffer);
                return false;
            }

            const bool minimized = draw_data->DisplaySize.x <= 0.0F || draw_data->DisplaySize.y <= 0.0F;
            if (swapchain_texture != nullptr && !minimized) {
                ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, command_buffer);

                const auto& clear_color = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
                SDL_GPUColorTargetInfo target_info{};
                target_info.texture     = swapchain_texture;
                target_info.clear_color = { clear_color.x, clear_color.y, clear_color.z, clear_color.w };
                target_info.load_op     = SDL_GPU_LOADOP_CLEAR;
                target_info.store_op    = SDL_GPU_STOREOP_STORE;

                auto* render_pass = SDL_BeginGPURenderPass(command_buffer, &target_info, 1, nullptr);
                if (render_pass == nullptr) {
                    SDL_CancelGPUCommandBuffer(command_buffer);
                    return false;
                }

                ImGui_ImplSDLGPU3_RenderDrawData(draw_data, command_buffer, render_pass);
                SDL_EndGPURenderPass(render_pass);
            }

            return SDL_SubmitGPUCommandBuffer(command_buffer);
        }

    } // namespace

    int run(int /*argc*/, char* /*argv*/[]) {
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
            std::fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
            return 1;
        }

        auto* window = SDL_CreateWindow(
            "FileMonitor",
            initial_width,
            initial_height,
            SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY
        );
        if (window == nullptr) {
            std::fprintf(stderr, "Window creation failed: %s\n", SDL_GetError());
            SDL_Quit();
            return 1;
        }

        SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        SDL_ShowWindow(window);

        constexpr auto shader_formats = SDL_GPU_SHADERFORMAT_SPIRV |
                                        SDL_GPU_SHADERFORMAT_DXIL |
                                        SDL_GPU_SHADERFORMAT_MSL |
                                        SDL_GPU_SHADERFORMAT_METALLIB;
        auto* gpu_device = SDL_CreateGPUDevice(shader_formats, false, nullptr);
        if (gpu_device == nullptr) {
            std::fprintf(stderr, "GPU device creation failed: %s\n", SDL_GetError());
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }

        if (!SDL_ClaimWindowForGPUDevice(gpu_device, window)) {
            std::fprintf(stderr, "GPU window claim failed: %s\n", SDL_GetError());
            SDL_DestroyGPUDevice(gpu_device);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }

        if (!SDL_SetGPUSwapchainParameters(
                gpu_device,
                window,
                SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                SDL_GPU_PRESENTMODE_VSYNC
            )) {
            std::fprintf(stderr, "GPU swapchain setup failed: %s\n", SDL_GetError());
            SDL_ReleaseWindowFromGPUDevice(gpu_device, window);
            SDL_DestroyGPUDevice(gpu_device);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        auto& io        = ImGui::GetIO();
        io.IniFilename  = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ApplicationState state;
        apply_theme(state.dark_mode);

        if (!ImGui_ImplSDL3_InitForSDLGPU(window)) {
            std::fprintf(stderr, "ImGui SDL3 backend initialization failed\n");
            ImGui::DestroyContext();
            SDL_ReleaseWindowFromGPUDevice(gpu_device, window);
            SDL_DestroyGPUDevice(gpu_device);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }

        ImGui_ImplSDLGPU3_InitInfo init_info{};
        init_info.Device               = gpu_device;
        init_info.ColorTargetFormat    = SDL_GetGPUSwapchainTextureFormat(gpu_device, window);
        init_info.MSAASamples          = SDL_GPU_SAMPLECOUNT_1;
        init_info.SwapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;
        init_info.PresentMode          = SDL_GPU_PRESENTMODE_VSYNC;
        if (!ImGui_ImplSDLGPU3_Init(&init_info)) {
            std::fprintf(stderr, "ImGui SDL_GPU backend initialization failed: %s\n", SDL_GetError());
            ImGui_ImplSDL3_Shutdown();
            ImGui::DestroyContext();
            SDL_ReleaseWindowFromGPUDevice(gpu_device, window);
            SDL_DestroyGPUDevice(gpu_device);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }

        bool running = true;
        int exit_code = 0;
        while (running) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                ImGui_ImplSDL3_ProcessEvent(&event);
                if (event.type == SDL_EVENT_QUIT ||
                    (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                     event.window.windowID == SDL_GetWindowID(window))) {
                    running = false;
                }
            }

            if (!running) {
                break;
            }

            if ((SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) != 0) {
                SDL_Delay(10);
                continue;
            }

            ImGui_ImplSDLGPU3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();

            render_workspace(state);

            if (!render_frame(gpu_device, window)) {
                std::fprintf(stderr, "GPU frame submission failed: %s\n", SDL_GetError());
                exit_code = 1;
                break;
            }
        }

        SDL_WaitForGPUIdle(gpu_device);
        ImGui_ImplSDLGPU3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();

        SDL_ReleaseWindowFromGPUDevice(gpu_device, window);
        SDL_DestroyGPUDevice(gpu_device);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return exit_code;
    }

} // namespace file_monitor::ui
