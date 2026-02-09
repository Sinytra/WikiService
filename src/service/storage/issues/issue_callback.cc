#include "issue_callback.h"

#include "issue_service.h"

using namespace drogon;

namespace service {
    ProjectIssueCallback::ProjectIssueCallback(const std::string &id, const std::shared_ptr<spdlog::logger> &log) :
        deploymentId_(id), logger_(log), hasErrors_(false) {}

    Task<> addIssueStatic(const ProjectIssueLevel level, const ProjectIssueType type, const ProjectError subject, const std::string details,
                          const std::string file, const std::string deploymentId, const std::shared_ptr<spdlog::logger> logger) {
        if (!deploymentId.empty()) {
            co_await global::issues->addProjectIssueInternal(deploymentId, level, type, subject, details, file);
        }

        const auto logLevel = level == ProjectIssueLevel::ERROR ? spdlog::level::err : spdlog::level::warn;
        const auto logDetail = details.empty() ? "" : " '" + details + "' ";
        const auto logFile = file.empty() ? "" : " in file " + file;
        logger->log(logLevel, "[Issue] {} / {}{}{}", enumToStr(type), enumToStr(subject), logDetail, logFile);
    }

    Task<> ProjectIssueCallback::addIssue(const ProjectIssueLevel level, const ProjectIssueType type, const ProjectError subject,
                                          const std::string details, const std::string file) {
        if (level == ProjectIssueLevel::ERROR)
            hasErrors_ = true;

        co_await addIssueStatic(level, type, subject, details, file, deploymentId_, logger_);
    }

    void ProjectIssueCallback::addIssueAsync(const ProjectIssueLevel level, const ProjectIssueType type, const ProjectError subject,
                                             const std::string details, const std::string file) {
        if (level == ProjectIssueLevel::ERROR)
            hasErrors_ = true;

        const auto currentLoop = trantor::EventLoop::getEventLoopOfCurrentThread();
        currentLoop->queueInLoop(
            async_func([level, type, subject, details, file, deploymentId = std::string(deploymentId_), logger = logger_]() -> Task<> {
                co_await addIssueStatic(level, type, subject, details, file, deploymentId, logger);
            }));
    }

    bool ProjectIssueCallback::hasErrors() const { return hasErrors_; }

    ProjectFileIssueCallback::ProjectFileIssueCallback(const std::shared_ptr<ProjectIssueCallback> &issues,
                                                       const std::filesystem::path &path) :
        issues_(issues), absolutePath_(path), path_(path) {}

    ProjectFileIssueCallback::ProjectFileIssueCallback(const ProjectFileIssueCallback &issues, const std::filesystem::path &path) :
        issues_(issues.issues_), absolutePath_(path), path_(relative(path, issues.path_)) {}

    Task<> ProjectFileIssueCallback::addIssue(const ProjectIssueLevel level, const ProjectIssueType type, const ProjectError subject,
                                              const std::string details) const {
        co_await issues_->addIssue(level, type, subject, details, path_.string());
    }

    void ProjectFileIssueCallback::addIssueAsync(const ProjectIssueLevel level, const ProjectIssueType type, const ProjectError subject,
                                                 const std::string &details) const {
        issues_->addIssueAsync(level, type, subject, details, path_.string());
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
}
