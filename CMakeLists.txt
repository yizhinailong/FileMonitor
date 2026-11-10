cmake_minimum_required(VERSION 4.3.0)

project(
    FileMonitor
    VERSION 0.5.1
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

file(GLOB_RECURSE CORE_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/*.hpp"
)

file(GLOB_RECURSE GUI_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gui/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gui/*.hpp"
)

file(GLOB_RECURSE TUI_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/tui/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/tui/*.hpp"
)

add_library(
    ${PROJECT_NAME}Core
    STATIC
    ${CORE_SOURCES}
)

target_include_directories(
    ${PROJECT_NAME}Core
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(
    ${PROJECT_NAME}Core
    PRIVATE
    ${CORE_DEPENDENCIES}
)

add_executable(
    ${PROJECT_NAME}
    ${GUI_SOURCES}
    app.rc
)

set_target_properties(
    ${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    WIN32_EXECUTABLE TRUE
)

target_compile_definitions(
    ${PROJECT_NAME}
    PRIVATE
    FILE_MONITOR_VERSION=${PROJECT_VERSION}
)

target_link_libraries(
    ${PROJECT_NAME}
    PRIVATE
    ${PROJECT_NAME}Core
    ${GUI_DEPENDENCIES}
)

add_executable(
    ${PROJECT_NAME}Tui
    ${TUI_SOURCES}
    app.rc
)

set_target_properties(
    ${PROJECT_NAME}Tui PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_compile_definitions(
    ${PROJECT_NAME}Tui
    PRIVATE
    FILE_MONITOR_VERSION=${PROJECT_VERSION}
)

target_link_libraries(
    ${PROJECT_NAME}Tui
    PRIVATE
    ${PROJECT_NAME}Core
    ${TUI_DEPENDENCIES}
)

install(
    TARGETS ${PROJECT_NAME} ${PROJECT_NAME}Tui
    BUNDLE DESTINATION .
    RUNTIME DESTINATION .
)
