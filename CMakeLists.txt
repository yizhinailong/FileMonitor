cmake_minimum_required(VERSION 4.3.0)

project(FileMonitor
    VERSION 0.3.8
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(WIN32)
    enable_language(RC)
endif()

include(cmake/Dependencies.cmake)

add_executable(${PROJECT_NAME}
    src/core/ChangeLogger.cpp
    src/core/ChangeLogger.hpp
    src/core/Configuration.cpp
    src/core/Configuration.hpp
    src/core/DirectoryMonitor.cpp
    src/core/DirectoryMonitor.hpp
    src/core/FileSystem.cpp
    src/core/FileSystem.hpp
    src/main.cpp
    src/ui/Application.cpp
    src/ui/Application.hpp
    src/ui/MainWindow.cpp
    src/ui/MainWindow.hpp
    src/ui/SdlImGuiBackend.cpp
    src/ui/SdlImGuiBackend.hpp
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

if(WIN32)
    target_sources(${PROJECT_NAME} PRIVATE app.rc)
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
elseif(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE TRUE)
endif()

target_include_directories(${PROJECT_NAME}
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    ${PROJECT_DEPENDENCIES}
)

install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    RUNTIME DESTINATION .
)
