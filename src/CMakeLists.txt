add_executable(${PROJECT_NAME}
    core/Configuration.cpp
    core/Configuration.hpp
    core/DirectoryMonitor.cpp
    core/DirectoryMonitor.hpp
    core/FileSystem.cpp
    core/FileSystem.hpp
    main.cpp
    ui/Application.cpp
    ui/Application.hpp
    ui/MainWindow.cpp
    ui/MainWindow.hpp
    ui/SdlImGuiBackend.cpp
    ui/SdlImGuiBackend.hpp
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
elseif(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE TRUE)
endif()

target_include_directories(${PROJECT_NAME}
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    ${PROJECT_DEPENDENCIES}
)

install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    RUNTIME DESTINATION .
)
