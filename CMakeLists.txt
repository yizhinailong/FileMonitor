cmake_minimum_required(VERSION 4.3.0)

project(FileMonitor
    VERSION 0.2.6
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(cmake/Dependencies.cmake)

add_subdirectory(src)
