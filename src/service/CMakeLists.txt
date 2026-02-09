read_and_escape_schema("../../builtin/minecraft/recipe_type/crafting.json" CRAFTING)
read_and_escape_schema("../../builtin/minecraft/recipe_type/smelting.json" SMELTING)
read_and_escape_schema("../../builtin/minecraft/recipe_type/blasting.json" BLASTING)
read_and_escape_schema("../../builtin/minecraft/recipe_type/campfire_cooking.json" CAMPFIRE_COOKING)
read_and_escape_schema("../../builtin/minecraft/recipe_type/smoking.json" SMOKING)
read_and_escape_schema("../../builtin/minecraft/recipe_type/stonecutting.json" STONECUTTING)
read_and_escape_schema("../../builtin/minecraft/recipe_type/smithing_transform.json" SMITHING_TRANSFORM)

configure_file(storage/ingestor/recipe/builtin_recipe_type.h.in
        ${CMAKE_CURRENT_BINARY_DIR}/service/storage/ingestor/recipe/builtin_recipe_type.h
        @ONLY
)

add_library(service STATIC
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

        project/virtual/format.cc
        project/virtual/virtual.cc
        project/cached/cached.cc
        project/content.cc
        project/format.cc
        project/frontmatter.cc
        project/pages.cc
        project/recipe_resolver.cc
        project/reports.cc
        project/resolved.cc

        storage/ingestor/ingestor.cc
        storage/ingestor/ingestor_content_paths.cc
        storage/ingestor/ingestor_metadata.cc
        storage/ingestor/ingestor_recipes.cc
        storage/ingestor/ingestor_tags.cc
        storage/ingestor/recipe/recipe_builtin.cc
        storage/ingestor/recipe/recipe_custom.cc
        storage/ingestor/recipe/recipe_parser.cc
        storage/issues/issue_callback.cc
        storage/issues/issue_service.cc
        storage/issues/issues.cc
        storage/management/project_access.cc
        storage/management/project_management.cc
        storage/deployment.cc
        storage/storage.cc
        storage/gitclone.cc
        storage/gitops.cc
        storage/realtime.cc

        system/access_keys.cc
        system/game_data.cc
        system/lang.cc
        system/startup.cc

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
        pugixml::pugixml
        tiny-process-library::tiny-process-library
)

target_include_directories(service PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
)