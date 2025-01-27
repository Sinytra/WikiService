add_library(service STATIC
        auth.cc
        cache.cc
        cloudflare.cc
        database.cc
        github.cc
        platforms.cc
        resolved.cc
        schemas.cc
        storage.cc
        util.cc
)

target_link_libraries(service PRIVATE
        log
        Drogon::Drogon
        nlohmann_json_schema_validator
        libgit2
        libgit2package
        OpenSSL::Crypto
)

target_include_directories(service PUBLIC
        ${PROJECT_SOURCE_DIR}
)
