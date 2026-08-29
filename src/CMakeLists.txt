qt_add_executable(appuntitled
    main.cpp
)

add_subdirectory(core)
add_subdirectory(qml)

qt_add_qml_module(appuntitled
    URI untitled
    VERSION 1.0
    QML_FILES
    ${UNTITLED_QML_FILES}
)

set_target_properties(appuntitled PROPERTIES
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appuntitled
    PRIVATE
    Qt6::Quick
    untitled::core
)

install(TARGETS appuntitled
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
