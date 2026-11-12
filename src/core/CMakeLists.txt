file(GLOB_RECURSE CORE_MODULES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cppm"
)

add_library(
    ${PROJECT_NAME}Core
    STATIC
)

target_sources(
    ${PROJECT_NAME}Core
    PUBLIC
    FILE_SET CXX_MODULES
    FILES ${CORE_MODULES}
)

target_link_libraries(
    ${PROJECT_NAME}Core
    PRIVATE
    ${CORE_DEPENDENCIES}
)
