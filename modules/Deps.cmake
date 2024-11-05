include(modules/CPM.cmake)

#CPMAddPackage(
#        NAME SPDLOG
#        GITHUB_REPOSITORY gabime/spdlog
#        GIT_TAG v1.11.0
#        OPTIONS
#        "SPDLOG_BUILD_SHARED OFF"
#        "SPDLOG_BUILD_EXAMPLE OFF"
#        "SPDLOG_BUILD_EXAMPLE_HO OFF"
#        "SPDLOG_BUILD_TESTS OFF"
#        "SPDLOG_BUILD_TESTS_HO OFF"
#        "SPDLOG_BUILD_BENCH OFF"
#        "SPDLOG_SANITIZE_ADDRESS OFF"
#        "SPDLOG_INSTALL OFF"
#        "SPDLOG_FMT_EXTERNAL OFF"
#        "SPDLOG_FMT_EXTERNAL_HO OFF"
#        "SPDLOG_NO_EXCEPTIONS OFF"
#)

CPMAddPackage(
        NAME jwt-cpp
        GITHUB_REPOSITORY Thalhammer/jwt-cpp
        GIT_TAG v0.7.0
        OPTIONS
        "JWT_BUILD_EXAMPLES OFF"
)

CPMAddPackage(
        NAME base64
        GITHUB_REPOSITORY tobiaslocker/base64
        GIT_TAG "387b32f337b83d358ac1ffe574e596ba99c41d31"
)
if (base64_ADDED)
    add_library(base64 INTERFACE)
    target_include_directories(base64 INTERFACE ${base64_SOURCE_DIR}/include)
endif()