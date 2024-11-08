#pragma once

#include <drogon/HttpController.h>
#include <service/github.h>
#include "service/service.h"
#include "service/github.h"
#include "service/database.h"
#include "service/documentation.h"

#include "service/service.h"

namespace api::v1
{
    class HelloWorld : public drogon::HttpController<HelloWorld, false>
    {
    public:
        explicit HelloWorld(service::Service&, service::GitHub&, service::Database&, service::Documentation&);

    public:
        METHOD_LIST_BEGIN
            ADD_METHOD_TO(HelloWorld::status, "/status?mod={1}&path={2}", drogon::Get);
        METHOD_LIST_END

        drogon::Task<> status(drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback,
                                      std::string mod, std::string path) const;

    private:
        service::Service& service_;
        service::GitHub& github_;
        service::Database& database_;
        service::Documentation& documentation_;
    };
}
