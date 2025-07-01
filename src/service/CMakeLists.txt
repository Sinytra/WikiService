function(read_and_escape_schema input_file output_var)
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/${input_file}" temp_schema)
    string(CONFIGURE "${temp_schema}" ${output_var} ESCAPE_QUOTES)
    set(${output_var} "${temp_schema}" PARENT_SCOPE)
endfunction()

read_and_escape_schema("schemas/sinytra-wiki.schema.json" PROJECT_META_SCHEMA)
read_and_escape_schema("schemas/_meta.schema.json" FOLDER_META_SCHEMA)
read_and_escape_schema("schemas/config.schema.json" SYSTEM_CONFIG_SCHEMA)
read_and_escape_schema("schemas/game-recipe.schema.json" GAME_RECIPE_SCHEMA)
read_and_escape_schema("schemas/game-tag.schema.json" GAME_TAG_SCHEMA)
read_and_escape_schema("schemas/game-data.schema.json" GAME_DATA_EXPORT_SCHEMA)
read_and_escape_schema("schemas/game-recipe-type.schema.json" GAME_RECIPE_TYPE_SCHEMA)
read_and_escape_schema("schemas/game-recipe-base.schema.json" BASE_GAME_RECIPE_SCHEMA)
read_and_escape_schema("schemas/game-recipe-custom.schema.json" CUSTOM_GAME_RECIPE_SCHEMA)
read_and_escape_schema("schemas/workbenches.schema.json" RECIPE_WORKBENCHES)

read_and_escape_schema("../../builtin/minecraft/recipe_type/crafting.json" CRAFTING)
read_and_escape_schema("../../builtin/minecraft/recipe_type/smelting.json" SMELTING)
read_and_escape_schema("../../builtin/minecraft/recipe_type/blasting.json" BLASTING)
read_and_escape_schema("../../builtin/minecraft/recipe_type/campfire_cooking.json" CAMPFIRE_COOKING)

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
        content/game_data.cc
        content/ingestor_content_paths.cc
        content/ingestor_metadata.cc
        content/ingestor_tags.cc
        content/ingestor_recipes.cc
        content/recipe_builtin.cc
        content/recipe_custom.cc
        content/recipe_parser.cc
        content/recipe_resolver.cc
        database/database.cc
        database/database_content.cc
        database/database_system.cc
        database/database_deployments.cc
        database/database_issues.cc
        database/database_reports.cc
        database/resolved_db.cc
        lang/crowdin.cc
        lang/lang.cc
        storage/storage.cc
        storage/gitops.cc
        storage/realtime.cc
        system_data/system_data_import.cc
        auth.cc
        cache.cc
        cloudflare.cc
        crypto.cc
        deployment.cc
        github.cc
        platforms.cc
        project_issue.cc
        resolved.cc
        reports.cc
        schemas.cc
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
        libzip::zip
)

target_include_directories(service PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
)