add_executable(${PROJECT_NAME}
    main.cpp
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
elseif(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE TRUE)
endif()

add_subdirectory(core)
add_subdirectory(ui)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    ${PROJECT_NAME}_core
    ${PROJECT_NAME}_ui
    SDL3::SDL3-static
)

install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    RUNTIME DESTINATION .
)
