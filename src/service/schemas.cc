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

nlohmann::json schemas::projectMetadata = R"(
{
    "$schema": "https://json-schema.org/draft/2020-12/schema",
    "title": "Sinytra Wiki Documentation Metadata",
    "description": "Provides documentation project meta information",
    "type": "object",
    "properties": {
        "id": {
            "description": "Unique project ID that identifies the project across the wiki. We recommend using your projectid.",
            "type": "string",
            "minLength": 2,
            "maxLength": 126,
            "pattern": "^[a-z]+[a-z0-9-]+$"
        },
        "platforms": {
            "description": "A map of your project's available distribution platforms to platform project slugs.",
            "type": "object",
            "additionalProperties": { "type": "string" }
        },
        "versions": {
            "description": "A map of additional minecraft versions to git branch names to make available.",
            "type": "object",
            "additionalProperties": { "type": "string" }
        },
        "platform": {
            "description": "Your project's distribution platform. Deprecated: use 'platforms' instead",
            "enum": ["curseforge", "modrinth"],
            "deprecated": true
        },
        "slug": {
            "description": "The host platform's project slug. Deprecated: use 'platforms' instead",
            "type": "string",
            "deprecated": true
        }
    },
    "allOf": [
        {
            "required": ["id"]
        },
        {
            "oneOf": [
                {
                    "required": ["platform", "slug"],
                    "deprecated": true
                },
                {
                    "required": ["platforms"]
                }
            ]
        }
    ]
}
)"_json;

nlohmann::json schemas::folderMetadata = R"(
{
    "$schema": "https://json-schema.org/draft/2020-12/schema",
    "title": "Sinytra Wiki Folder Layout Metadata",
    "description": "Provides file naming information for a directory",
    "type": "object",
    "required": [],
    "additionalProperties": {
        "oneOf": [
            {
                "type": "object",
                "properties": {
                    "name": {
                        "type": "string",
                        "description": "Page display name"
                    },
                    "icon": {
                        "type": ["string", "null"],
                        "description": "Display icon name"
                    }
                }
            },
            {
                "type": "string"
            }
        ]
    }
}
)"_json;

nlohmann::json schemas::repositoryMigration = R"(
{
    "$schema": "https://json-schema.org/draft/2020-12/schema",
    "title": "Repository migration",
    "type": "object",
    "properties": {
        "repo": { "type": "string" }
    },
    "required": ["repo"]
}
)"_json;

nlohmann::json schemas::systemConfig = R"(
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
        "cloudflare": {
            "type": "object",
            "properties": {
                "token": { "type": "string" },
                "account_tag": { "type": "string" },
                "site_tag": { "type": "string"}
            },
            "required": ["token", "account_tag", "site_tag"]
        },
        "curseforge_key": { "type": "string" },
        "api_key": { "type": "string" }
    },
    "required": ["github_app"]
}
)"_json;