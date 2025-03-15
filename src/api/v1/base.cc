#include "base.h"
#include "error.h"

using namespace drogon;
using namespace service;

namespace api::v1 {
    Task<std::optional<ResolvedProject>>
    BaseProjectController::getProjectWithParams(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                                const std::string project) {
        const auto version = req->getOptionalParameter<std::string>("version");
        const auto locale = req->getOptionalParameter<std::string>("locale");
        co_return co_await getProject(project, version, locale, callback);
    }

    Task<std::optional<ResolvedProject>>
    BaseProjectController::getVersionedProject(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                               const std::string project) {
        const auto version = req->getOptionalParameter<std::string>("version");
        co_return co_await getProject(project, version, std::nullopt, callback);
    }

    Task<std::optional<ResolvedProject>>
    BaseProjectController::getProject(const std::string &project, const std::optional<std::string> &version,
                                      const std::optional<std::string> &locale,
                                      const std::function<void(const HttpResponsePtr &)> callback) {
        if (project.empty()) {
            errorResponse(Error::ErrBadRequest, "Missing project parameter", callback);
            co_return std::nullopt;
        }

        const auto [proj, projErr] = co_await global::database->getProjectSource(project);
        if (!proj) {
            errorResponse(projErr, "Project not found", callback);
            co_return std::nullopt;
        }

        const auto [resolved, resErr](co_await global::storage->getProject(*proj, version, locale));
        if (!resolved) {
            errorResponse(resErr, "Resolution failure", callback);
            co_return std::nullopt;
        }

        co_return *resolved;
    }
}
