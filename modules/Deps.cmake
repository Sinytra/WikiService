include(modules/CPM.cmake)

IF (WIN32 AND USE_LOCAL_DEPS)
    CPMAddPackage(
            NAME SPDLOG
            GITHUB_REPOSITORY gabime/spdlog
            GIT_TAG v1.11.0
    )

    CPMAddPackage(
            NAME Hiredis
            GITHUB_REPOSITORY redis/hiredis
            GIT_TAG v1.2.0
            OPTIONS
            "BUILD_SHARED_LIBS OFF"
    )
    if (Hiredis_ADDED)
        add_library(Hiredis_lib ALIAS hiredis)

        file(MAKE_DIRECTORY ${hiredis_SOURCE_DIR}/include/hiredis)
        file(GLOB HIREDIS_HEADERS "${hiredis_SOURCE_DIR}/*.h")
        foreach (header ${HIREDIS_HEADERS})
            configure_file(${header} ${hiredis_SOURCE_DIR}/include/hiredis COPYONLY)
        endforeach ()

        target_include_directories(hiredis PUBLIC
                $<BUILD_INTERFACE:${hiredis_SOURCE_DIR}/include>
                $<INSTALL_INTERFACE:include>
        )
    endif ()

    CPMAddPackage(
            NAME jsoncpp
            GITHUB_REPOSITORY open-source-parsers/jsoncpp
            GIT_TAG 1.9.6
    )
    if (jsoncpp_ADDED)
        add_library(Jsoncpp_lib ALIAS jsoncpp_static)
    endif ()

    CPMAddPackage(
            NAME drogon
            GITHUB_REPOSITORY drogonframework/drogon
            GIT_TAG v1.9.8
            OPTIONS
            "BUILD_EXAMPLES OFF"
            "BUILD_SHARED_LIBS OFF"
            "BUILD_REDIS ON"
    )
    if (drogon_ADDED)
        add_library(Drogon::Drogon ALIAS drogon)
    endif ()
ENDIF ()

CPMAddPackage(
        NAME nlohmann_json
        GITHUB_REPOSITORY nlohmann/json
        GIT_TAG v3.12.0
        OPTIONS
        "JSON_BuildTests OFF"
        "BUILD_SHARED_LIBS OFF"
)

CPMAddPackage(
        NAME nlohmann_json_schema_validator
        GITHUB_REPOSITORY pboettch/json-schema-validator
        GIT_TAG 2.3.0
        OPTIONS
        "JSON_VALIDATOR_INSTALL OFF"
        "JSON_VALIDATOR_BUILD_EXAMPLES OFF"
        "JSON_VALIDATOR_BUILD_TESTS OFF"
        "JSON_VALIDATOR_SHARED_LIBS OFF"
)

CPMAddPackage(
        NAME libgit2
        GITHUB_REPOSITORY libgit2/libgit2
        GIT_TAG v1.9.0
        OPTIONS
        "BUILD_TESTS OFF"
        "BUILD_SHARED_LIBS OFF"
)

CPMAddPackage(
        NAME yaml-cpp
        GITHUB_REPOSITORY jbeder/yaml-cpp
        GIT_TAG 0.8.0
        OPTIONS
        "YAML_BUILD_SHARED_LIBS OFF"
)

CPMAddPackage(
        NAME sentry
        VERSION 0.9.1
        URL https://github.com/getsentry/sentry-native/releases/download/0.9.1/sentry-native.zip
        URL_HASH SHA256=e5349b1a233ac52291e54cba3a6d028781d8173e8b3cd759f17cd27769f02eab
        OPTIONS
        "SENTRY_BACKEND crashpad"
)

if (sentry_ADDED)
    set_target_properties(sentry PROPERTIES C_STANDARD 11 C_EXTENSIONS ON)
endif()

CPMAddPackage(
        NAME pugixml
        GITHUB_REPOSITORY zeux/pugixml
        GIT_TAG v1.15
        OPTIONS
        "BUILD_SHARED_LIBS OFF"
)