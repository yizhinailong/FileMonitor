include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/CPM.cmake")

CPMAddPackage(
    URI "gh:gabime/spdlog@1.17.0"
    OPTIONS
    "SPDLOG_BUILD_EXAMPLE OFF"
    "SPDLOG_BUILD_TESTS OFF"
    "SPDLOG_BUILD_BENCH OFF"
    "SPDLOG_INSTALL OFF"
)

CPMAddPackage(
    NAME SDL3
    GITHUB_REPOSITORY libsdl-org/SDL
    GIT_TAG release-3.4.14
    OPTIONS
    "SDL_SHARED OFF"
    "SDL_STATIC ON"
    "SDL_TEST_LIBRARY OFF"
    "SDL_TESTS OFF"
    "SDL_EXAMPLES OFF"
    "SDL_DISABLE_INSTALL ON"
    "SDL_DISABLE_INSTALL_DOCS ON"
    "SDL_INSTALL_TESTS OFF"
)

CPMAddPackage(
    NAME imgui
    GITHUB_REPOSITORY ocornut/imgui
    GIT_TAG v1.92.9
    DOWNLOAD_ONLY YES
)

add_library(imgui STATIC
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_sdlgpu3.cpp"
)
add_library(imgui::imgui ALIAS imgui)

target_include_directories(imgui
    SYSTEM PUBLIC
    "${imgui_SOURCE_DIR}"
    "${imgui_SOURCE_DIR}/backends"
)

target_link_libraries(imgui
    PUBLIC
    SDL3::SDL3-static
)
