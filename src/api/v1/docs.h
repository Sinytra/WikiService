#pragma once

#include <drogon/HttpController.h>

#include "service/database.h"
#include "service/documentation.h"
#include "service/github.h"

namespace api::v1 {
    class DocsController : public drogon::HttpController<DocsController, false> {
    public:
        explicit DocsController(service::GitHub &, service::Database &, service::Documentation &);

    public:
        METHOD_LIST_BEGIN
        ADD_METHOD_TO(DocsController::page, "/api/v1/mod/{1:mod}/page/.*", drogon::Get, "AuthFilter");
        ADD_METHOD_TO(DocsController::tree, "/api/v1/mod/{1:mod}/tree", drogon::Get, "AuthFilter");
        ADD_METHOD_TO(DocsController::asset, "/api/v1/mod/{1:mod}/asset/.*", drogon::Get, "AuthFilter");
        ADD_METHOD_TO(DocsController::invalidate, "/api/v1/mod/{1:mod}/invalidate", drogon::Post, "AuthFilter");
        METHOD_LIST_END

        drogon::Task<> page(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback, std::string mod) const;

        drogon::Task<> tree(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback,
                            std::string mod) const;

        drogon::Task<> asset(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback, std::string mod) const;

        drogon::Task<> invalidate(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback,
                                  std::string mod) const;

    private:
        service::GitHub &github_;
        service::Database &database_;
        service::Documentation &documentation_;
    };
}
