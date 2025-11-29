#pragma once

#include <drogon/HttpController.h>

#include <service/storage/storage.h>

namespace api::v1 {
    class BaseProjectController {
    public:
        static drogon::Task<service::ResolvedProject>
        getProjectWithParams(drogon::HttpRequestPtr req, std::string project);

        static drogon::Task<std::shared_ptr<service::ProjectBase>>
        getProjectWithParamsCached(drogon::HttpRequestPtr req, std::string project);

        static drogon::Task<service::ResolvedProject>
        getVersionedProject(drogon::HttpRequestPtr req, std::string project);

        static drogon::Task<service::ResolvedProject> getProject(const std::string &project, const std::optional<std::string> &version,
                                                                 const std::optional<std::string> &locale);

        static drogon::Task<Project> getUserProject(drogon::HttpRequestPtr req, std::string id);

        static drogon::Task<service::ResolvedProject> getUserProject(drogon::HttpRequestPtr req, const std::string &project,
                                                                     const std::optional<std::string> &version,
                                                                     const std::optional<std::string> &locale);

        static nlohmann::json jsonBody(const drogon::HttpRequestPtr &req);
        static nlohmann::json validatedBody(const drogon::HttpRequestPtr &req, const nlohmann::json &schema);
    };
}
