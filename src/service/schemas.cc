#include "schemas.h"

auto schemas::projectRegister = R"(
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

auto schemas::projectUpdate = R"(
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

auto schemas::projectMetadata = R"(@PROJECT_META_SCHEMA@)"_json;

auto schemas::folderMetadata = R"(@FOLDER_META_SCHEMA@)"_json;

auto schemas::systemConfig = R"(@SYSTEM_CONFIG_SCHEMA@)"_json;