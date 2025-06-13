#include "base.h"

#include <auth.h>

using namespace drogon;
using namespace service;

namespace api::v1 {
    Task<ResolvedProject>
    BaseProjectController::getProjectWithParams(const HttpRequestPtr req, const std::string project) {
        const auto version = req->getOptionalParameter<std::string>("version");
        const auto locale = req->getOptionalParameter<std::string>("locale");
        co_return co_await getProject(project, version, locale);
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

        const auto [proj, projErr] = co_await global::database->getProjectSource(project);
        if (!proj) {
            throw ApiException(projErr, "Project not found");
        }

        const auto [resolved, resErr](co_await global::storage->getProject(*proj, version, locale));
        if (!resolved) {
            throw ApiException(resErr, "Resolution failure");
        }

        co_return *resolved;
    }

    Task<Project> BaseProjectController::getUserProject(HttpRequestPtr req, const std::string id) {
        const auto session{co_await global::auth->getSession(req)};
        const auto project{co_await global::database->getUserProject(session.username, id)};
        if (!project) {
            throw ApiException(Error::ErrBadRequest, "not_found");
        }
        co_return *project;
    }

    Task<ResolvedProject> BaseProjectController::getUserProject(const HttpRequestPtr req, const std::string &id,
                                                                const std::optional<std::string> &version,
                                                                const std::optional<std::string> &locale) {
        const auto session{co_await global::auth->getSession(req)};
        const auto project{co_await global::database->getUserProject(session.username, id)};
        if (!project) {
            throw ApiException(Error::ErrBadRequest, "not_found");
        }

        const auto [resolved, resErr](co_await global::storage->getProject(*project, version, locale));
        if (!resolved) {
            throw ApiException(Error::ErrBadRequest, "not_found");
        }

        co_return *resolved;
    }
}
