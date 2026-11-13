cmake_minimum_required(VERSION 4.5.0)

project(
    FileMonitor
    VERSION 0.6.0
    LANGUAGES C CXX
)

if(NOT WIN32)
    message(FATAL_ERROR "FileMonitor only supports Windows")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

enable_language(RC)

include(cmake/ClangClCcache.cmake)
include(cmake/Options.cmake)
include(cmake/Dependencies.cmake)

add_subdirectory(src/core)

set(FILE_MONITOR_APPS)

if(FILE_MONITOR_BUILD_GUI)
    add_subdirectory(src/gui)
    list(APPEND FILE_MONITOR_APPS ${PROJECT_NAME})
endif()

if(FILE_MONITOR_BUILD_TUI)
    add_subdirectory(src/tui)
    list(APPEND FILE_MONITOR_APPS ${PROJECT_NAME}Tui)
endif()

install(
    TARGETS ${FILE_MONITOR_APPS}
    BUNDLE DESTINATION .
    RUNTIME DESTINATION .
)
