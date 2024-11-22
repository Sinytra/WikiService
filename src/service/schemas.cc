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
        "is_community": { "type": "boolean" },
        "mr_code": { "type": "string" }
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

auto schemas::projectMetadata = R"(
{
    "$schema": "https://json-schema.org/draft/2020-12/schema",
    "title": "Wiki Documentation Metadata",
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

auto schemas::systemConfig = R"(
{
    "$schema": "https://json-schema.org/draft/2020-12/schema",
    "title": "Modded MC Wiki System Configuration",
    "type": "object",
    "properties": {
        "github_app": {
            "type": "object",
            "properties": {
                "name": { "type": "string" },
                "client_id": { "type": "string" },
                "private_key_path": { "type": "string"}
            },
            "required": ["name", "client_id", "private_key_path"]
        },
        "modrinth_app": {
            "type": "object",
            "properties": {
                "client_id": { "type": "string" },
                "client_secret": { "type": "string" },
                "redirect_url": { "type": "string" }
            }
        },
        "curseforge_key": { "type": "string" },
        "api_key": { "type": "string" }
    },
    "required": ["github_app"]
}
)"_json;
