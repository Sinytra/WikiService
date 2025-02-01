add_library(service STATIC
        content/game_content.cc
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

file(GLOB SCHEMA_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.json")
add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/schemas.cc
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/schemas.cc ${CMAKE_CURRENT_BINARY_DIR}/schemas.cc
        DEPENDS ${SCHEMA_FILES}
)
add_custom_target(update_schemas DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/schemas.cc)
add_dependencies(service update_schemas)

function(read_and_escape_schema input_file output_var)
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/schemas/${input_file}" temp_schema)
    string(CONFIGURE "${temp_schema}" ${output_var} ESCAPE_QUOTES)
    set(${output_var} "${temp_schema}" PARENT_SCOPE)
endfunction()

read_and_escape_schema("sinytra-wiki.schema.json" PROJECT_META_SCHEMA)
read_and_escape_schema("_meta.schema.json" FOLDER_META_SCHEMA)
read_and_escape_schema("config.schema.json" SYSTEM_CONFIG_SCHEMA)
read_and_escape_schema("game-recipe.schema.json" GAME_RECIPE_SCHEMA)

configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/schemas.cc
        ${CMAKE_CURRENT_BINARY_DIR}/schemas.cc
        @ONLY
)