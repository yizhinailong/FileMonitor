file(GLOB_RECURSE GUI_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
)

add_executable(
    ${PROJECT_NAME}
    ${GUI_SOURCES}
    "${PROJECT_SOURCE_DIR}/app.rc"
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

target_include_directories(
    ${PROJECT_NAME}
    PRIVATE
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(
    ${PROJECT_NAME}
    PRIVATE
    ${PROJECT_NAME}Core
    ${GUI_DEPENDENCIES}
)
