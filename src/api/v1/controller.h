#pragma once

#include <drogon/HttpController.h>
#include <service/github.h>

#include "service/service.h"

namespace api::v1
{
    class HelloWorld : public drogon::HttpController<HelloWorld, false>
    {
    public:
        explicit HelloWorld(service::Service&);

    public:
        METHOD_LIST_BEGIN
            ADD_METHOD_TO(HelloWorld::status, "/status", drogon::Get);
        METHOD_LIST_END

        drogon::Task<> status(drogon::HttpRequestPtr req,
            std::function<void(const drogon::HttpResponsePtr&)> callback) const;

    private:
        service::Service& service_;
    };
}
