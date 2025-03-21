#pragma once

#include <drogon/HttpController.h>

#include "service/database.h"
#include "service/storage.h"

namespace api::v1 {
    class BaseProjectController {
    public:
        static drogon::Task<service::ResolvedProject>
        getProjectWithParams(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback,
                             std::string project);

        static drogon::Task<service::ResolvedProject>
        getVersionedProject(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback, std::string project);

        static drogon::Task<service::ResolvedProject> getProject(const std::string &project, const std::optional<std::string> &version,
                                                                 const std::optional<std::string> &locale,
                                                                 std::function<void(const drogon::HttpResponsePtr &)> callback);

        static drogon::Task<Project> getUserProject(drogon::HttpRequestPtr req, const std::string id);

        static drogon::Task<service::ResolvedProject> getUserProject(drogon::HttpRequestPtr req, const std::string &project,
                                                                     const std::optional<std::string> &version,
                                                                     const std::optional<std::string> &locale,
                                                                     std::function<void(const drogon::HttpResponsePtr &)> callback);
    };
}
