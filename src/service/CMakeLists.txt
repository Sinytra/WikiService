file(READ "${CMAKE_CURRENT_SOURCE_DIR}/schemas/sinytra-wiki.schema.json" PROJECT_META_SCHEMA)
string(CONFIGURE "${PROJECT_META_SCHEMA}" PROJECT_META_SCHEMA ESCAPE_QUOTES)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/schemas/_meta.schema.json" FOLDER_META_SCHEMA)
string(CONFIGURE "${FOLDER_META_SCHEMA}" FOLDER_META_SCHEMA ESCAPE_QUOTES)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/schemas/config.schema.json" SYSTEM_CONFIG_SCHEMA)
string(CONFIGURE "${SYSTEM_CONFIG_SCHEMA}" SYSTEM_CONFIG_SCHEMA ESCAPE_QUOTES)

configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/schemas.cc
        ${CMAKE_CURRENT_BINARY_DIR}/schemas.cc
        @ONLY
)

add_library(service STATIC
        auth.cc
        cache.cc
        cloudflare.cc
        crypto.cc
        database.cc
        github.cc
        platforms.cc
        resolved.cc
        schemas.cc
        storage.cc
        util.cc
        ${CMAKE_CURRENT_BINARY_DIR}/schemas.cc
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
        ${CMAKE_CURRENT_SOURCE_DIR}
)