#include "base.h"

#include <auth.h>
#include <service/system/lang.h>

#include "project/cached.h"

using namespace drogon;
using namespace service;

namespace api::v1 {
    Task<std::shared_ptr<ProjectBase>> BaseProjectController::getProjectWithParamsCached(const HttpRequestPtr req, const std::string project) {
        const auto resolved(co_await getProjectWithParams(req, project));
        const auto shared(std::make_shared<ResolvedProject>(resolved));
        const auto cached(std::make_shared<CachedProject>(shared));
        co_return cached;
    }

    Task<ResolvedProject> BaseProjectController::getProjectWithParams(const HttpRequestPtr req, const std::string project) {
        const auto version = req->getOptionalParameter<std::string>("version");
        const auto locale = req->getOptionalParameter<std::string>("locale");
        const auto validatedLocale = co_await validateLocale(locale);
        co_return co_await getProject(project, version, validatedLocale);
    }

    Task<ResolvedProject> BaseProjectController::getVersionedProject(const HttpRequestPtr req, const std::string project) {
        const auto version = req->getOptionalParameter<std::string>("version");
        co_return co_await getProject(project, version, std::nullopt);
    }

    Task<ResolvedProject> BaseProjectController::getProject(const std::string &project, const std::optional<std::string> &version,
                                                            const std::optional<std::string> &locale) {
        if (project.empty()) {
            throw ApiException(Error::ErrBadRequest, "Missing project parameter");
        }

        const auto [resolved, resErr](co_await global::storage->getProject(project, version, locale));
        if (!resolved) {
            throw ApiException(resErr, "Resolution failure");
        }

        co_return *resolved;
    }

    Task<Project> BaseProjectController::getUserProject(HttpRequestPtr req, const std::string id) {
        const auto session{co_await global::auth->getSession(req)};
        const auto project{co_await global::database->getUserProject(session.username, id)};
        if (!project) {
            throw ApiException(Error::ErrNotFound, "not_found");
        }
        co_return *project;
    }

    Task<ResolvedProject> BaseProjectController::getUserProject(const HttpRequestPtr req, const std::string &id,
                                                                const std::optional<std::string> &version,
                                                                const std::optional<std::string> &locale) {
        const auto session{co_await global::auth->getSession(req)};
        const auto project{co_await global::database->getUserProject(session.username, id)};
        if (!project) {
            throw ApiException(Error::ErrNotFound, "not_found");
        }

        const auto [resolved, resErr](co_await global::storage->getProject(*project, version, locale));
        if (!resolved) {
            throw ApiException(Error::ErrNotFound, "not_found");
        }

        co_return *resolved;
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
