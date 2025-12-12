read_and_escape_schema("definition/sinytra-wiki.schema.json" PROJECT_META_SCHEMA)
read_and_escape_schema("definition/_meta.schema.json" FOLDER_META_SCHEMA)
read_and_escape_schema("definition/config.schema.json" SYSTEM_CONFIG_SCHEMA)
read_and_escape_schema("definition/game-recipe.schema.json" GAME_RECIPE_SCHEMA)
read_and_escape_schema("definition/game-tag.schema.json" GAME_TAG_SCHEMA)
read_and_escape_schema("definition/game-recipe-type.schema.json" GAME_RECIPE_TYPE_SCHEMA)
read_and_escape_schema("definition/game-recipe-base.schema.json" BASE_GAME_RECIPE_SCHEMA)
read_and_escape_schema("definition/game-recipe-custom.schema.json" CUSTOM_GAME_RECIPE_SCHEMA)
read_and_escape_schema("definition/workbenches.schema.json" RECIPE_WORKBENCHES)
read_and_escape_schema("definition/properties.schema.json" PROPERTIES)

configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/schemas.cc
        ${CMAKE_CURRENT_BINARY_DIR}/schemas.cc
        @ONLY
)

add_library(schemas STATIC
        schemas.cc
        ${CMAKE_CURRENT_BINARY_DIR}/schemas.cc
)

target_link_libraries(schemas PRIVATE
        nlohmann_json
)

target_include_directories(schemas PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
)