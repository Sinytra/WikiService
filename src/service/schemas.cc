#include "schemas.h"

nlohmann::json schemas::projectRegister = R"(
{
    "$schema": "https://json-schema.org/draft/2020-12/schema",
    "title": "Project registration",
    "type": "object",
    "properties": {
        "repo": { "type": "string" },
        "branch": { "type": "string" },
        "path": { "type": "string" },
        "is_community": { "type": "boolean" },
        "mr_code": { "type": "string" }
    },
    "required": ["repo", "branch", "path"]
}
)"_json;

nlohmann::json schemas::projectMetadata = R"(
{
    "$schema": "https://json-schema.org/draft/2020-12/schema",
    "title": "Sinytra Wiki Documentation Metadata",
    "description": "Provides documentation project meta information",
    "type": "object",
    "properties": {
        "id": {
            "description": "Unique project ID that identifies the project across the wiki. We recommend using your modid.",
            "type": "string",
            "minLength": 2,
            "maxLength": 126,
            "pattern": "^[a-z]+[a-z0-9-]+$"
        },
        "platform": {
            "description": "Your mod's distribution platform.",
            "enum": ["curseforge", "modrinth"]
        },
        "slug": {
            "description": "The host platform's project slug.",
            "type": "string"
        },
        "versions": {
            "description": "A map of additional minecraft versions to git branch names to make available.",
            "type": "object",
            "additionalProperties": { "type": "string" }
        }
    },
    "required": ["id", "platform", "slug"]
}
)"_json;
