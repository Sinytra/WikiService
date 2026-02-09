#include "issues.h"

#include <service/util.h>

using namespace drogon;

namespace service {
    // clang-format off
    ENUM_FROM_TO_STR(ProjectIssueLevel,
        {ProjectIssueLevel::WARNING, "warning"},
        {ProjectIssueLevel::ERROR, "error"}
    )

    ENUM_FROM_TO_STR(ProjectIssueType,
        {ProjectIssueType::META, "meta"},
        {ProjectIssueType::FILE, "file"},
        {ProjectIssueType::GIT_CLONE, "git_clone"},
        {ProjectIssueType::GIT_INFO, "git_info"},
        {ProjectIssueType::INGESTOR, "ingestor"},
        {ProjectIssueType::PAGE, "page"},
        {ProjectIssueType::INTERNAL, "internal"}
    )

    ENUM_FROM_TO_STR(ProjectError,
        {ProjectError::OK, "ok"},
        {ProjectError::REQUIRES_AUTH, "requires_auth"},
        {ProjectError::NO_REPOSITORY, "no_repository"},
        {ProjectError::REPO_TOO_LARGE, "repo_too_large"},
        {ProjectError::NO_BRANCH, "no_branch"},
        {ProjectError::NO_PATH, "no_path"},
        {ProjectError::INVALID_META, "invalid_meta"},
        {ProjectError::PAGE_RENDER, "page_render"},
        {ProjectError::DUPLICATE_PAGE, "duplicate_page"},
        {ProjectError::UNKNOWN_RECIPE_TYPE, "unknown_recipe_type"},
        {ProjectError::INVALID_INGREDIENT, "invalid_ingredient"},
        {ProjectError::INVALID_FILE, "invalid_file"},
        {ProjectError::INVALID_FORMAT, "invalid_format"},
        {ProjectError::INVALID_RESLOC, "invalid_resloc"},
        {ProjectError::INVALID_VERSION_BRANCH, "invalid_version_branch"},
        {ProjectError::MISSING_PLATFORM_PROJECT, "missing_platform_project"},
        {ProjectError::NO_PAGE_TITLE, "no_page_title"},
        {ProjectError::INVALID_FRONTMATTER, "invalid_frontmatter"},
        {ProjectError::MISSING_REQUIRED_ATTRIBUTE, "missing_required_attribute"}
    )
    // clang-format on
}
