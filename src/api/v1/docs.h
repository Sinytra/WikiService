#pragma once

#include <drogon/HttpController.h>

#include "service/database.h"
#include "service/storage.h"

namespace api::v1 {
    class DocsController final : public drogon::HttpController<DocsController, false> {
    public:
        explicit DocsController(Database &, Storage &);

        METHOD_LIST_BEGIN
        ADD_METHOD_TO(DocsController::project, "/api/v1/project/{1:project}", drogon::Get, "AuthFilter");
        ADD_METHOD_TO(DocsController::page, "/api/v1/project/{1:project}/page/.*", drogon::Get, "AuthFilter");
        ADD_METHOD_TO(DocsController::tree, "/api/v1/project/{1:project}/tree", drogon::Get, "AuthFilter");
        ADD_METHOD_TO(DocsController::asset, "/api/v1/project/{1:project}/asset/.*", drogon::Get); // Public
        METHOD_LIST_END

        drogon::Task<> project(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback,
                               std::string project) const;

        drogon::Task<> page(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback,
                            std::string project) const;

        drogon::Task<> tree(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback,
                            std::string project) const;

        drogon::Task<> asset(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback,
                             std::string project) const;

    private:
        Database &database_;
        Storage &storage_;

        drogon::Task<std::optional<ResolvedProject>> getProject(const std::string &project, const std::optional<std::string> &version,
                                                                const std::optional<std::string> &locale,
                                                                std::function<void(const drogon::HttpResponsePtr &)> callback) const;
    };
}
