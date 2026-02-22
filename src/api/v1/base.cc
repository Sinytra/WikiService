#include "base.h"

#include <auth.h>
#include <service/project/cached/cached.h>
#include <service/system/lang.h>

using namespace drogon;
using namespace service;
using namespace logging;

namespace api::v1 {
    void requireNonVirtual(const ProjectBasePtr &project) {
        if (project && project->getProject().getValueOfIsVirtual()) {
            throw ApiException(Error::ErrNotFound, "Project not found");
        }
    }

    void assertNonEmptyParam(const std::string &str) {
        if (str.empty()) {
            throw ApiException(Error::ErrBadRequest, "Insufficient parameters");
        }
    }

    void notFound(const std::string &msg) { throw ApiException(Error::ErrNotFound, msg); }

    Task<> checkUserAccess(const HttpRequestPtr req, const Project project) {
        auto visibility = parseProjectVisibility(project.getValueOfVisibility());
        if (visibility == ProjectVisibility::UNKNOWN) {
            logger.error("Project {} has unknown visiibility. Bug?", project.getValueOfId());
            visibility = ProjectVisibility::PRIVATE;
        }

        // Non-private: Everyone can access
        if (visibility != ProjectVisibility::PRIVATE) {
            co_return;
        }

        // Private: User must be authenticated and part of project members
        const auto session(co_await global::auth->maybeGetSession(req));
        if (!session || !isAdmin(*session) || !co_await global::database->getUserProject(session->username, project.getValueOfId())) {
            notFound();
        }
    }

    Task<ProjectBasePtr> BaseProjectController::getProjectWithParamsCached(const HttpRequestPtr req, const std::string project) {
        const auto resolved(co_await getProjectWithParams(req, project));
        co_return std::make_shared<CachedProject>(resolved);
    }

    Task<ProjectBasePtr> BaseProjectController::getProjectWithParams(const HttpRequestPtr req, const std::string project) {
        const auto version = req->getOptionalParameter<std::string>("version");
        const auto locale = req->getOptionalParameter<std::string>("locale");
        const auto validatedLocale = co_await validateLocale(locale);
        co_return co_await getProject(req, project, version, validatedLocale);
    }

    Task<ProjectBasePtr> BaseProjectController::getProject(const HttpRequestPtr req, const std::string project,
                                                           const std::optional<std::string> version,
                                                           const std::optional<std::string> locale) {
        assertNonEmptyParam(project);

        const auto resolved = co_await global::storage->getProject(project, version, locale);
        if (!resolved) {
            throw ApiException(resolved.error(), "Resolution failure");
        }

        co_await checkUserAccess(req, (*resolved)->getProject());

        co_return *resolved;
    }

    Task<Project> BaseProjectController::getUserProject(HttpRequestPtr req, const std::string id) {
        const auto session(co_await global::auth->getSession(req));
        const auto project = co_await getUserProject(session, id);
        if (!project) {
            notFound();
        }
        co_return *project;
    }

    Task<ProjectBasePtr> BaseProjectController::getUserProject(const HttpRequestPtr req, const std::string id,
                                                               const std::optional<std::string> version,
                                                               const std::optional<std::string> locale) {
        const auto session(co_await global::auth->getSession(req));
        const auto project = co_await getUserProject(session, id);
        if (!project) {
            notFound();
        }

        const auto resolved(co_await global::storage->getProject(*project, version, locale));
        if (!resolved) {
            notFound();
        }

        co_return *resolved;
    }

    Task<TaskResult<Project>> BaseProjectController::getUserProject(const UserSession session, const std::string id) {
        const auto project = isAdmin(session) ? co_await global::database->getProjectSource(id)
                                              : co_await global::database->getUserProject(session.username, id);
        if (!project) {
            notFound();
        }
        co_return *project;
    }

    nlohmann::json BaseProjectController::jsonBody(const HttpRequestPtr &req) {
        const auto json(req->getJsonObject());
        if (!json) {
            throw ApiException(Error::ErrBadRequest, req->getJsonError());
        }
        return parkourJson(*json);
    }

    nlohmann::json BaseProjectController::validatedBody(const HttpRequestPtr &req, const nlohmann::json &schema) {
        const auto json(jsonBody(req));
        if (const auto error = validateJson(schema, json)) {
            throw ApiException(Error::ErrBadRequest, error->msg);
        }
        return json;
    }
}
