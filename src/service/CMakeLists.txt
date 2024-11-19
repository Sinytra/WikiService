add_library(service STATIC
        github.cc
        util.cc
        cache.cc
        database.cc
        documentation.cc
        platforms.cc
        schemas.cc
)

target_link_libraries(service PRIVATE
        log
        drogon
        jwt-cpp
        base64
        nlohmann_json_schema_validator
)

target_include_directories(service PUBLIC
        ${PROJECT_SOURCE_DIR}
)
