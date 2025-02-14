#pragma once

#include <drogon/HttpController.h>

#include "base.h"

namespace api::v1 {
    class DocsController final : public drogon::HttpController<DocsController, false>, public BaseProjectController {
    public:
        METHOD_LIST_BEGIN
        ADD_METHOD_TO(DocsController::project,  "/api/v1/docs/{1:project}",           drogon::Get, "AuthFilter");
        ADD_METHOD_TO(DocsController::page,     "/api/v1/docs/{1:project}/page/.*",   drogon::Get, "AuthFilter");
        ADD_METHOD_TO(DocsController::tree,     "/api/v1/docs/{1:project}/tree",      drogon::Get, "AuthFilter");
        // Public
        ADD_METHOD_TO(DocsController::asset,    "/api/v1/docs/{1:project}/asset/.*",  drogon::Get);
        METHOD_LIST_END

        drogon::Task<> project(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback,
                               std::string project) const;

        drogon::Task<> page(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback,
                            std::string project) const;

        drogon::Task<> tree(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback,
                            std::string project) const;

        drogon::Task<> asset(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback,
                             std::string project) const;
    };
}
