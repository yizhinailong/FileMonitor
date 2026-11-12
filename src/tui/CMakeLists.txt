file(GLOB_RECURSE TUI_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
)

add_executable(
    ${PROJECT_NAME}Tui
    ${TUI_SOURCES}
    "${PROJECT_SOURCE_DIR}/app.rc"
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

target_include_directories(
    ${PROJECT_NAME}Tui
    PRIVATE
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(
    ${PROJECT_NAME}Tui
    PRIVATE
    ${PROJECT_NAME}Core
    ${TUI_DEPENDENCIES}
)
