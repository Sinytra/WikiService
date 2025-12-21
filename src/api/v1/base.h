#pragma once

#include <drogon/HttpController.h>
#include <service/auth.h>
#include <service/storage/storage.h>

#define ADMIN_AUTH "AuthFilter", "AdminFilter"

namespace api::v1 {
    void requireNonVirtual(const service::ProjectBasePtr &project);

    void assertNonEmptyParam(const std::string &str);

    void notFound(const std::string &msg = "not_found");

    template<typename T>
    void assertFound(T &&t, const std::string &&msg = "not_found") {
        if (!t) {
            notFound(std::move(msg));
        }
    }

    class BaseProjectController {
    public:
        static drogon::Task<service::ProjectBasePtr> getProjectWithParams(drogon::HttpRequestPtr req, std::string project);

        static drogon::Task<service::ProjectBasePtr> getProjectWithParamsCached(drogon::HttpRequestPtr req, std::string project);

        static drogon::Task<service::ProjectBasePtr> getVersionedProject(drogon::HttpRequestPtr req, std::string project);

        static drogon::Task<service::ProjectBasePtr> getProject(const std::string &project, const std::optional<std::string> &version,
                                                                const std::optional<std::string> &locale);

        static drogon::Task<Project> getUserProject(drogon::HttpRequestPtr req, std::string id);

        static drogon::Task<service::ProjectBasePtr> getUserProject(drogon::HttpRequestPtr req, const std::string &project,
                                                                    const std::optional<std::string> &version,
                                                                    const std::optional<std::string> &locale);

        static drogon::Task<TaskResult<Project>> getUserProject(service::UserSession session, std::string id);

        static nlohmann::json jsonBody(const drogon::HttpRequestPtr &req);
        static nlohmann::json validatedBody(const drogon::HttpRequestPtr &req, const nlohmann::json &schema);
    };
}
