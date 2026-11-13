file(GLOB_RECURSE TUI_MODULES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cppm"
)

add_executable(
    ${PROJECT_NAME}Tui
    "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
    "${PROJECT_SOURCE_DIR}/app.rc"
)

target_sources(
    ${PROJECT_NAME}Tui
    PRIVATE
    FILE_SET CXX_MODULES
    FILES ${TUI_MODULES}
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
