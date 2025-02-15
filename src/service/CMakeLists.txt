function(read_and_escape_schema input_file output_var)
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/${input_file}" temp_schema)
    string(CONFIGURE "${temp_schema}" ${output_var} ESCAPE_QUOTES)
    set(${output_var} "${temp_schema}" PARENT_SCOPE)
endfunction()

read_and_escape_schema("schemas/sinytra-wiki.schema.json" PROJECT_META_SCHEMA)
read_and_escape_schema("schemas/_meta.schema.json" FOLDER_META_SCHEMA)
read_and_escape_schema("schemas/config.schema.json" SYSTEM_CONFIG_SCHEMA)
read_and_escape_schema("schemas/game-recipe.schema.json" GAME_RECIPE_SCHEMA)

read_and_escape_schema("../../builtin/minecraft/recipe_type/crafting_shaped.json" SHAPED_CRAFTING)

configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/schemas.cc
        ${CMAKE_CURRENT_BINARY_DIR}/schemas.cc
        @ONLY
)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/content/builtin_recipe_type.h.in
        ${CMAKE_CURRENT_BINARY_DIR}/content/builtin_recipe_type.h
        @ONLY
)

add_library(service STATIC
        content/game_content.cc
        content/game_recipes.cc
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
        ${CMAKE_CURRENT_BINARY_DIR}
)