include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/CPM.cmake")

CPMAddPackage(
    URI "gh:gabime/spdlog@1.17.0"
    OPTIONS
    "SPDLOG_BUILD_EXAMPLE OFF"
    "SPDLOG_BUILD_TESTS OFF"
    "SPDLOG_BUILD_BENCH OFF"
    "SPDLOG_INSTALL OFF"
)
