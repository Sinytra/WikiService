#include "issues.h"

#include <service/database/database.h>
#include <service/util.h>
#include "models/ProjectIssue.h"

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

    ProjectIssueCallback::ProjectIssueCallback(const std::string &id, const std::shared_ptr<spdlog::logger> &log) :
        deploymentId_(id), logger_(log), hasErrors_(false) {}

    Task<> ProjectIssueCallback::addIssue(const ProjectIssueLevel level, const ProjectIssueType type, const ProjectError subject,
                                          const std::string details, const std::string file) {
        if (!deploymentId_.empty()) {
            co_await service::addIssue(deploymentId_, level, type, subject, details, file);
        }

        const auto logLevel = level == ProjectIssueLevel::ERROR ? spdlog::level::err : spdlog::level::warn;
        const auto logDetail = details.empty() ? "" : " '" + details + "' ";
        const auto logFile = file.empty() ? "" : " in file " + file;
        logger_->log(logLevel, "[Issue] {} / {}{}{}", enumToStr(type), enumToStr(subject), logDetail, logFile);

        if (level == ProjectIssueLevel::ERROR)
            hasErrors_ = true;
    }

    void ProjectIssueCallback::addIssueAsync(const ProjectIssueLevel level, const ProjectIssueType type, const ProjectError subject,
                                             const std::string details, const std::string file) {
        if (level == ProjectIssueLevel::ERROR)
            hasErrors_ = true;

        app().getLoop()->queueInLoop(
            async_func([*this, level, type, subject, details = std::string(details), file = std::string(file)]() mutable -> Task<> {
                co_await addIssue(level, type, subject, details, file);
            }));
    }

    bool ProjectIssueCallback::hasErrors() const { return hasErrors_; }

    ProjectFileIssueCallback::ProjectFileIssueCallback(ProjectIssueCallback &issues, const std::filesystem::path &path) :
        issues_(issues), absolutePath_(path), path_(path) {}

    ProjectFileIssueCallback::ProjectFileIssueCallback(const ProjectFileIssueCallback &issues, const std::filesystem::path &path) :
        issues_(issues.issues_), absolutePath_(path), path_(relative(path, issues.path_)) {}

    Task<> ProjectFileIssueCallback::addIssue(const ProjectIssueLevel level, const ProjectIssueType type, const ProjectError subject,
                                              const std::string details) const {
        co_await issues_.addIssue(level, type, subject, details, path_.string());
    }

    void ProjectFileIssueCallback::addIssueAsync(const ProjectIssueLevel level, const ProjectIssueType type, const ProjectError subject,
                                                 const std::string &details) const {
        issues_.addIssueAsync(level, type, subject, details, path_.string());
    }

    std::optional<ResourceLocation> ProjectFileIssueCallback::validateResourceLocation(const std::string &str) const {
        const auto parsed = ResourceLocation::parse(str);
        if (!parsed) {
            addIssueAsync(ProjectIssueLevel::ERROR, ProjectIssueType::INGESTOR, ProjectError::INVALID_RESLOC, str);
        }
        return parsed;
    }

    std::optional<nlohmann::json> ProjectFileIssueCallback::readAndValidateJson(const nlohmann::json &schema) const {
        const auto json = parseJsonFile(absolutePath_);
        if (!json) {
            addIssueAsync(ProjectIssueLevel::ERROR, ProjectIssueType::INGESTOR, ProjectError::INVALID_FILE);
            return std::nullopt;
        }

        if (const auto error = validateJson(schema, *json)) {
            addIssueAsync(ProjectIssueLevel::ERROR, ProjectIssueType::INGESTOR, ProjectError::INVALID_FORMAT, error->format());
            return std::nullopt;
        }

        return json;
    }

    Task<> addIssue(const std::string deploymentId, const ProjectIssueLevel level, const ProjectIssueType type, const ProjectError subject,
                    const std::string details, const std::string file) {
        ProjectIssue issue;
        issue.setDeploymentId(deploymentId);
        issue.setLevel(enumToStr(level));
        issue.setType(enumToStr(type));
        issue.setSubject(enumToStr(subject));
        issue.setDetails(details);
        issue.setFile(file);

        co_await global::database->addModel(issue);
    }
}
