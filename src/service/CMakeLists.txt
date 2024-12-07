add_library(service STATIC
        github.cc
        util.cc
        cache.cc
        database.cc
        documentation.cc
        platforms.cc
        schemas.cc
        cloudflare.cc
)

target_link_libraries(service PRIVATE
        log
        Drogon::Drogon
        jwt-cpp
        base64
        nlohmann_json_schema_validator
)

target_include_directories(service PUBLIC
        ${PROJECT_SOURCE_DIR}
)
