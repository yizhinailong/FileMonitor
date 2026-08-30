#include "Application.hpp"

#include <cstdio>

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>

namespace file_monitor::ui {
    namespace {

        constexpr int INITIAL_WIDTH{ 960 };
        constexpr int INITIAL_HEIGHT{ 640 };

        auto apply_theme() -> void {
            ImGui::StyleColorsDark();

            auto& style{ ImGui::GetStyle() };
            style.WindowRounding    = 0.0F;
            style.ChildRounding     = 0.0F;
            style.FrameRounding     = 6.0F;
            style.PopupRounding     = 6.0F;
            style.ScrollbarRounding = 6.0F;
            style.GrabRounding      = 6.0F;
            style.WindowBorderSize  = 0.0F;
            style.FrameBorderSize   = 0.0F;
            style.WindowPadding     = { 16.0F, 12.0F };
            style.FramePadding      = { 12.0F, 8.0F };
            style.ItemSpacing       = { 10.0F, 10.0F };
        }

        auto render_workspace() -> void {
            auto const* viewport{ ImGui::GetMainViewport() };
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);

            constexpr auto WINDOW_FLAGS{ ImGuiWindowFlags_NoDecoration |
                                         ImGuiWindowFlags_NoMove |
                                         ImGuiWindowFlags_NoSavedSettings |
                                         ImGuiWindowFlags_NoBringToFrontOnFocus };

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0F, 0.0F });
            ImGui::Begin("FileMonitorWorkspace", nullptr, WINDOW_FLAGS);
            ImGui::PopStyleVar();

            auto const available{ ImGui::GetContentRegionAvail() };
            auto const directory_pane_height{ available.y * 0.1F };

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0.0F, 0.0F });
            ImGui::BeginChild("DirectoryPane", { available.x, directory_pane_height }, true);
            ImGui::TextUnformatted("Select directory");
            ImGui::EndChild();

            ImGui::BeginChild("LogPane", { available.x, 0.0F }, true);
            ImGui::TextUnformatted("Logs");
            ImGui::EndChild();
            ImGui::PopStyleVar();

            ImGui::End();
        }

        auto render_frame(SDL_GPUDevice* gpu_device, SDL_Window* window) -> bool {
            ImGui::Render();

            auto* draw_data{ ImGui::GetDrawData() };
            auto* command_buffer{ SDL_AcquireGPUCommandBuffer(gpu_device) };
            if (command_buffer == nullptr) {
                return false;
            }

            SDL_GPUTexture* swapchain_texture{ nullptr };
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
                    return false;
                }

                ImGui_ImplSDLGPU3_RenderDrawData(draw_data, command_buffer, render_pass);
                SDL_EndGPURenderPass(render_pass);
            }

            return SDL_SubmitGPUCommandBuffer(command_buffer);
        }

    } // namespace

    auto run(int /*argc*/, char* /*argv*/[]) -> int {
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
            std::fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
            return 1;
        }

        auto* window{ SDL_CreateWindow(
            "FileMonitor",
            INITIAL_WIDTH,
            INITIAL_HEIGHT,
            SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY
        ) };
        if (window == nullptr) {
            std::fprintf(stderr, "Window creation failed: %s\n", SDL_GetError());
            SDL_Quit();
            return 1;
        }

        SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        SDL_ShowWindow(window);

        constexpr auto SHADER_FORMATS{ SDL_GPU_SHADERFORMAT_SPIRV |
                                       SDL_GPU_SHADERFORMAT_DXIL |
                                       SDL_GPU_SHADERFORMAT_MSL |
                                       SDL_GPU_SHADERFORMAT_METALLIB };
        auto*          gpu_device{ SDL_CreateGPUDevice(SHADER_FORMATS, false, nullptr) };
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
        auto& io{ ImGui::GetIO() };
        io.IniFilename  = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        apply_theme();

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

        bool running{ true };
        int  exit_code{ 0 };
        while (running) {
            SDL_Event event{};
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

            render_workspace();

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
