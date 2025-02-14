#include "base.h"
#include "error.h"

#include <global.h>

using namespace drogon;
using namespace service;

namespace api::v1 {
    Task<std::optional<ResolvedProject>> BaseProjectController::getProject(const std::string &project, const std::optional<std::string> &version,
                                                                    const std::optional<std::string> &locale,
                                                                    const std::function<void(const HttpResponsePtr &)> callback) const {
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