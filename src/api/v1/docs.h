#pragma once

#include <drogon/HttpController.h>

#include "service/github.h"
#include "service/database.h"
#include "service/documentation.h"

namespace api::v1
{
    class DocsController : public drogon::HttpController<DocsController, false>
    {
    public:
        explicit DocsController(service::GitHub&, service::Database&, service::Documentation&);

    public:
        METHOD_LIST_BEGIN
            ADD_METHOD_TO(DocsController::page, "/page?mod={1}&path={2}", drogon::Get, "AuthFilter");
            ADD_METHOD_TO(DocsController::tree, "/tree?mod={1}", drogon::Get, "AuthFilter");
            ADD_METHOD_TO(DocsController::asset, "/asset?mod={1}&location={2}", drogon::Get, "AuthFilter");
        METHOD_LIST_END

        drogon::Task<> page(drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            std::string mod, std::string path) const;

        drogon::Task<> tree(drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            std::string mod) const;

        drogon::Task<> asset(drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            std::string mod, std::string location) const;

    private:
        service::GitHub& github_;
        service::Database& database_;
        service::Documentation& documentation_;
    };
}
