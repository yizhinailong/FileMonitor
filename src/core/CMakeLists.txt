add_library(${PROJECT_NAME}_core INTERFACE)

target_link_libraries(${PROJECT_NAME}_core
    INTERFACE
    spdlog::spdlog
)
