cmake_minimum_required(VERSION 4.4.0)

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
include(cmake/Dependencies.cmake)

add_subdirectory(src/core)
add_subdirectory(src/gui)
add_subdirectory(src/tui)

install(
    TARGETS ${PROJECT_NAME} ${PROJECT_NAME}Tui
    BUNDLE DESTINATION .
    RUNTIME DESTINATION .
)
