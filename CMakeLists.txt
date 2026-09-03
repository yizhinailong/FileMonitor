cmake_minimum_required(VERSION 4.3.0)

project(FileMonitor
    VERSION 0.4.5
    LANGUAGES C CXX
)

if(NOT WIN32)
    message(FATAL_ERROR "FileMonitor only supports Windows")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

enable_language(RC)

include(cmake/Dependencies.cmake)

add_executable(${PROJECT_NAME}
    src/core/ChangeLogger.cpp
    src/core/ChangeLogger.hpp
    src/core/Configuration.cpp
    src/core/Configuration.hpp
    src/core/DirectoryMonitor.cpp
    src/core/DirectoryMonitor.hpp
    src/core/directory_watch/DirectoryWatchBackend.hpp
    src/core/directory_watch/DirectoryWatchBackendWindows.cpp
    src/core/FileSystem.cpp
    src/core/FileSystem.hpp
    src/core/Utils.cpp
    src/core/Utils.hpp
    src/main.cpp
    src/ui/Application.cpp
    src/ui/Application.hpp
    src/ui/MainWindow.cpp
    src/ui/MainWindow.hpp
    src/ui/SdlImGuiBackend.cpp
    src/ui/SdlImGuiBackend.hpp
    src/ui/SettingsWindow.cpp
    src/ui/SettingsWindow.hpp
    app.rc
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    WIN32_EXECUTABLE TRUE
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
    FILE_MONITOR_VERSION=${PROJECT_VERSION}
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    ${PROJECT_DEPENDENCIES}
)

install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    RUNTIME DESTINATION .
)
