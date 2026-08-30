add_executable(${PROJECT_NAME}
    main.cpp
    ui/Application.cpp
    ui/Application.hpp
    ui/MainWindows.cpp
    ui/MainWindows.hpp
    ui/SdlImGuiBackend.cpp
    ui/SdlImGuiBackend.hpp
    ui/utils/Text.cpp
    ui/utils/Text.hpp
    ui/windows/DirectoryWindow.cpp
    ui/windows/DirectoryWindow.hpp
    ui/windows/LogWindow.cpp
    ui/windows/LogWindow.hpp
    ui/windows/Window.cpp
    ui/windows/Window.hpp
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
