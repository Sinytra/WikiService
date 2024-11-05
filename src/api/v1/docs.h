#pragma once

#include <drogon/HttpController.h>

#include "service/service.h"
#include "service/github.h"

namespace api::v1
{
    class DocsController : public drogon::HttpController<DocsController, false>
    {
    public:
        explicit DocsController(service::Service&, service::GitHub&);

    public:
        METHOD_LIST_BEGIN
            ADD_METHOD_TO(DocsController::page, "/page?mod={1}", drogon::Get);
        METHOD_LIST_END

        drogon::Task<> page(drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
            std::string mod) const;

    private:
        service::Service& service_;
        service::GitHub& github_;
    };
}
