read_and_escape_schema("../../builtin/minecraft/recipe_type/crafting.json" CRAFTING)
read_and_escape_schema("../../builtin/minecraft/recipe_type/smelting.json" SMELTING)
read_and_escape_schema("../../builtin/minecraft/recipe_type/blasting.json" BLASTING)
read_and_escape_schema("../../builtin/minecraft/recipe_type/campfire_cooking.json" CAMPFIRE_COOKING)
read_and_escape_schema("../../builtin/minecraft/recipe_type/smoking.json" SMOKING)
read_and_escape_schema("../../builtin/minecraft/recipe_type/stonecutting.json" STONECUTTING)
read_and_escape_schema("../../builtin/minecraft/recipe_type/smithing_transform.json" SMITHING_TRANSFORM)

configure_file(content/recipe/builtin_recipe_type.h.in
        ${CMAKE_CURRENT_BINARY_DIR}/service/content/recipe/builtin_recipe_type.h
        @ONLY
)

add_library(service STATIC
        content/ingestor/ingestor.cc
        content/ingestor/ingestor_content_paths.cc
        content/ingestor/ingestor_metadata.cc
        content/ingestor/ingestor_recipes.cc
        content/ingestor/ingestor_tags.cc
        content/recipe/recipe_builtin.cc
        content/recipe/recipe_custom.cc
        content/recipe/recipe_parser.cc
        content/recipe/recipe_resolver.cc

        database/database.cc
        database/database_content.cc
        database/database_system.cc
        database/database_deployments.cc
        database/database_issues.cc
        database/database_reports.cc
        database/project_database.cc

        external/crowdin.cc
        external/frontend.cc
        external/github.cc

        project/virtual/virtual.cc
        project/cached.cc
        project/content.cc
        project/format.cc
        project/frontmatter.cc
        project/pages.cc
        project/properties.cc
        project/reports.cc
        project/resolved.cc

        storage/deployment.cc
        storage/issues.cc
        storage/storage.cc
        storage/gitops.cc
        storage/realtime.cc

        system/access_keys.cc
        system/data_import.cc
        system/game_data.cc
        system/lang.cc

        util/crypto.cc

        auth.cc
        cache.cc
        platforms.cc
        util.cc
)

target_link_libraries(service PRIVATE
        log
        schemas
        Drogon::Drogon
        nlohmann_json_schema_validator
        libgit2
        libgit2package
        OpenSSL::Crypto
        libzip::zip
        yaml-cpp::yaml-cpp
)

target_include_directories(service PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
)