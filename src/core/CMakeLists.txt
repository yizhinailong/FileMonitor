add_library(untitled_core INTERFACE)
add_library(untitled::core ALIAS untitled_core)

target_link_libraries(untitled_core
    INTERFACE
    spdlog::spdlog
)
