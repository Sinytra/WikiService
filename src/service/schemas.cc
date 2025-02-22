//ReSharper disable CppUseAuto
#include "schemas.h"

#include <nlohmann/json-schema.hpp>

nlohmann::json schemas::projectRegister = R"(
{
    "$schema": "https://json-schema.org/draft/2020-12/schema",
    "title": "Project registration",
    "type": "object",
    "properties": {
        "repo": { "type": "string" },
        "branch": { "type": "string" },
        "path": { "type": "string" },
        "is_community": { "type": "boolean" }
    },
    "required": ["repo", "branch", "path"]
}
)"_json;

nlohmann::json schemas::projectUpdate = R"(
{
    "$schema": "https://json-schema.org/draft/2020-12/schema",
    "title": "Project update",
    "type": "object",
    "properties": {
        "repo": { "type": "string" },
        "branch": { "type": "string" },
        "path": { "type": "string" }
    },
    "required": ["repo", "branch", "path"]
}
)"_json;

nlohmann::json schemas::projectMetadata = R"(@PROJECT_META_SCHEMA@)"_json;

nlohmann::json schemas::folderMetadata = R"(@FOLDER_META_SCHEMA@)"_json;

nlohmann::json schemas::systemConfig = R"(@SYSTEM_CONFIG_SCHEMA@)"_json;

nlohmann::json schemas::gameRecipe = R"(@GAME_RECIPE_SCHEMA@)"_json;

nlohmann::json schemas::gameTag = R"(@GAME_TAG_SCHEMA@)"_json;