add_library(${PROJECT_NAME}_ui STATIC
    Application.cpp
    Application.hpp
)

target_include_directories(${PROJECT_NAME}_ui
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${PROJECT_NAME}_ui
    PRIVATE
    imgui::imgui
    SDL3::SDL3-static
)
