include_guard(GLOBAL)

option(FILE_MONITOR_BUILD_GUI "Build the SDL3 and Dear ImGui desktop application" ON)
option(FILE_MONITOR_BUILD_TUI "Build the FTXUI terminal application" ON)

if(NOT FILE_MONITOR_BUILD_GUI AND NOT FILE_MONITOR_BUILD_TUI)
    message(FATAL_ERROR "FILE_MONITOR_BUILD_GUI and FILE_MONITOR_BUILD_TUI are both OFF, so nothing would be built")
endif()
