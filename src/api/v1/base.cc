#include "base.h"

#include <auth.h>
#include <service/project/cached/cached.h>
#include <service/system/lang.h>

using namespace drogon;
using namespace service;

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

    Task<ProjectBasePtr> BaseProjectController::getProjectWithParamsCached(const HttpRequestPtr req, const std::string project) {
        const auto resolved(co_await getProjectWithParams(req, project));
        co_return std::make_shared<CachedProject>(resolved);
    }

    Task<ProjectBasePtr> BaseProjectController::getProjectWithParams(const HttpRequestPtr req, const std::string project) {
        const auto version = req->getOptionalParameter<std::string>("version");
        const auto locale = req->getOptionalParameter<std::string>("locale");
        const auto validatedLocale = co_await validateLocale(locale);
        co_return co_await getProject(project, version, validatedLocale);
    }

    Task<ProjectBasePtr> BaseProjectController::getVersionedProject(const HttpRequestPtr req, const std::string project) {
        const auto version = req->getOptionalParameter<std::string>("version");
        co_return co_await getProject(project, version, std::nullopt);
    }

    Task<ProjectBasePtr> BaseProjectController::getProject(const std::string &project, const std::optional<std::string> &version,
                                                           const std::optional<std::string> &locale) {
        assertNonEmptyParam(project);

        const auto resolved = co_await global::storage->getProject(project, version, locale);
        if (!resolved) {
            throw ApiException(resolved.error(), "Resolution failure");
        }

        co_return *resolved;
    }

    Task<Project> BaseProjectController::getUserProject(HttpRequestPtr req, const std::string id) {
        const auto session(co_await global::auth->getSession(req));
        const auto project = co_await getUserProject(session, id);
        if (!project) {
            throw ApiException(Error::ErrNotFound, "not_found");
        }
        co_return *project;
    }

    Task<ProjectBasePtr> BaseProjectController::getUserProject(const HttpRequestPtr req, const std::string &id,
                                                               const std::optional<std::string> &version,
                                                               const std::optional<std::string> &locale) {
        const auto session(co_await global::auth->getSession(req));
        const auto project = co_await getUserProject(session, id);
        if (!project) {
            throw ApiException(Error::ErrNotFound, "not_found");
        }

        const auto resolved(co_await global::storage->getProject(*project, version, locale));
        if (!resolved) {
            throw ApiException(Error::ErrNotFound, "not_found");
        }

        co_return *resolved;
    }

    Task<TaskResult<Project>> BaseProjectController::getUserProject(const UserSession session, const std::string id) {
        const auto isAdmin = session.user.getValueOfRole() == ROLE_ADMIN;
        const auto project =
            isAdmin ? co_await global::database->getProjectSource(id) : co_await global::database->getUserProject(session.username, id);
        if (!project) {
            throw ApiException(Error::ErrNotFound, "not_found");
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
