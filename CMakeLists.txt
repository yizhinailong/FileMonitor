cmake_minimum_required(VERSION 4.3.0)

project(untitled VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Quick)

qt_standard_project_setup(REQUIRES 6.10)

qt_add_executable(appuntitled
    main.cpp
)

qt_add_qml_module(appuntitled
    URI untitled
    QML_FILES
    Main.qml
)

set_target_properties(appuntitled PROPERTIES
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appuntitled
    PRIVATE Qt6::Quick
)

install(TARGETS appuntitled
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
