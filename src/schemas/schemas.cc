//ReSharper disable CppUseAuto
#include "schemas.h"

#include <nlohmann/json.hpp>

nlohmann::json schemas::getBulkProjects = R"(
{
    "$schema": "https://json-schema.org/draft/2020-12/schema",
    "title": "Get multiple projects by IDs",
    "type": "object",
    "properties": {
       "ids": {
           "type": "array",
           "items": {
               "type": "string"
           }
       }
    }
}
)"_json;

nlohmann::json schemas::projectRegister = R"(
{
    "$schema": "https://json-schema.org/draft/2020-12/schema",
    "title": "Project registration",
    "type": "object",
    "properties": {
        "repo": { "type": "string" },
        "branch": { "type": "string" },
        "path": { "type": "string" }
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

nlohmann::json schemas::projectIssue = R"(
{
    "$schema": "https://json-schema.org/draft/2020-12/schema",
    "title": "Project page issue",
    "type": "object",
    "properties": {
        "level": { "type": "string" },
        "type": { "type": "string" },
        "subject": { "type": "string" },
        "path": { "type": "string" },
        "details": { "type": "string" }
    },
    "required": ["level", "type", "subject", "details"]
}
)"_json;

nlohmann::json schemas::report = R"(
{
    "$schema": "https://json-schema.org/draft/2020-12/schema",
    "title": "Project report",
    "type": "object",
    "properties": {
        "project_id": { "type": "string" },
        "path": { "type": "string" },
        "reason": { "type": "string" },
        "body": { "type": "string" },
        "locale": { "type": "string" },
        "version": { "type": "string" },
        "type": { "type": "string" }
    },
    "required": ["project_id", "reason", "body", "type"]
}
)"_json;

nlohmann::json schemas::ruleReport = R"(
{
    "$schema": "https://json-schema.org/draft/2020-12/schema",
    "title": "Project report",
    "type": "object",
    "properties": {
        "resolution": { "type": "string", "enum": [ "accept", "dismiss" ] }
    },
    "required": ["resolution"]
}
)"_json;

nlohmann::json schemas::accessKey = R"(
{
    "$schema": "https://json-schema.org/draft/2020-12/schema",
    "title": "Access Key Creation",
    "type": "object",
    "properties": {
        "name": { "type": "string" },
        "days_valid": { "type": "integer", "minimum": 0, "maximum": 365 }
    },
    "required": ["name"]
}
)"_json;

nlohmann::json schemas::projectMetadata = R"(@PROJECT_META_SCHEMA@)"_json;

nlohmann::json schemas::folderMetadata = R"(@FOLDER_META_SCHEMA@)"_json;

nlohmann::json schemas::systemConfig = R"(@SYSTEM_CONFIG_SCHEMA@)"_json;

nlohmann::json schemas::gameRecipe = R"(@GAME_RECIPE_SCHEMA@)"_json;

nlohmann::json schemas::gameTag = R"(@GAME_TAG_SCHEMA@)"_json;

nlohmann::json schemas::gameDataExport = R"(@GAME_DATA_EXPORT_SCHEMA@)"_json;

nlohmann::json schemas::gameRecipeType = R"(@GAME_RECIPE_TYPE_SCHEMA@)"_json;

nlohmann::json schemas::gameRecipeBase = R"(@BASE_GAME_RECIPE_SCHEMA@)"_json;

nlohmann::json schemas::gameRecipeCustom = R"(@CUSTOM_GAME_RECIPE_SCHEMA@)"_json;

nlohmann::json schemas::recipeWorkbenches = R"(@RECIPE_WORKBENCHES@)"_json;

nlohmann::json schemas::properties = R"(@PROPERTIES@)"_json;
