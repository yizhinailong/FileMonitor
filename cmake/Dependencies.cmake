include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/CPM.cmake")

CPMAddPackage(
    NAME spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.17.0
    EXCLUDE_FROM_ALL YES
    SYSTEM YES
    OPTIONS
    "SPDLOG_BUILD_EXAMPLE OFF"
    "SPDLOG_BUILD_TESTS OFF"
    "SPDLOG_BUILD_BENCH OFF"
    "SPDLOG_INSTALL OFF"
)

# v3.12.0 has no C++ module interface. The experimental "import nlohmann.json;"
# support is only on develop, so pin a commit instead of the release tag.
CPMAddPackage(
    NAME json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG 5bad6c518bf6663b69f505511a19c98c91f77c3a
    EXCLUDE_FROM_ALL YES
    SYSTEM YES
    OPTIONS
    "JSON_BuildTests OFF"
    "JSON_Install OFF"
    "NLOHMANN_JSON_BUILD_MODULES ON"
)

if(FILE_MONITOR_BUILD_GUI)
    CPMAddPackage(
        NAME SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.4.20
        OPTIONS
        "SDL_SHARED OFF"
        "SDL_STATIC ON"
        "SDL_TEST_LIBRARY OFF"
        "SDL_TESTS OFF"
        "SDL_EXAMPLES OFF"
        "SDL_DISABLE_INSTALL ON"
        "SDL_DISABLE_INSTALL_DOCS ON"
        "SDL_INSTALL_TESTS OFF"
        "SDL_AUDIO OFF"
        "SDL_CAMERA OFF"
        "SDL_GPU OFF"
        "SDL_HAPTIC OFF"
        "SDL_HIDAPI OFF"
        "SDL_JOYSTICK OFF"
        "SDL_POWER OFF"
        "SDL_RENDER OFF"
        "SDL_SENSOR OFF"
        "SDL_TRAY OFF"
        "SDL_VULKAN OFF"
        "SDL_OPENGLES OFF"
        # Skip SDL's doc-comment warnings: with clang-cl they need
        # /clang:-fcomment-block-commands, which ccache treats as unsupported.
        "HAVE_GCC_WDOCUMENTATION OFF"
        "HAVE_GCC_WDOCUMENTATION_UNKNOWN_COMMAND OFF"
        "HAVE_CLANG_COMMENT_BLOCK_COMMANDS OFF"
    )

    CPMAddPackage(
        NAME imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.93.0
        DOWNLOAD_ONLY YES
    )

    find_package(OpenGL REQUIRED)

    add_library(imgui STATIC
        "${imgui_SOURCE_DIR}/imgui.cpp"
        "${imgui_SOURCE_DIR}/imgui_draw.cpp"
        "${imgui_SOURCE_DIR}/imgui_tables.cpp"
        "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
        "${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp"
        "${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp"
        "${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp"
    )
    add_library(imgui::imgui ALIAS imgui)

    target_include_directories(imgui
        SYSTEM PUBLIC
        "${imgui_SOURCE_DIR}"
        "${imgui_SOURCE_DIR}/backends"
    )

    target_link_libraries(imgui
        PUBLIC
        OpenGL::GL
        SDL3::SDL3-static
    )

    set(GUI_DEPENDENCIES
        imgui::imgui
        OpenGL::GL
        SDL3::SDL3-static
    )
endif()

if(FILE_MONITOR_BUILD_TUI)
    CPMAddPackage(
        NAME ftxui
        GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
        GIT_TAG v7.0.3
        EXCLUDE_FROM_ALL YES
        SYSTEM YES
        OPTIONS
        "FTXUI_BUILD_DOCS OFF"
        "FTXUI_BUILD_EXAMPLES OFF"
        "FTXUI_BUILD_TESTS OFF"
        "FTXUI_ENABLE_INSTALL OFF"
        "FTXUI_QUIET ON"
        "FTXUI_BUILD_MODULES ON"
    )

    set(TUI_DEPENDENCIES
        ftxui::modules
    )
endif()

set(CORE_DEPENDENCIES
    nlohmann_json_modules
    spdlog::spdlog
)
