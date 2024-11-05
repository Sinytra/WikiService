#pragma once

#include "service.h"

namespace service
{
    class ServiceImpl : public Service
    {
    public:
        ServiceImpl();

    private:
        Error status() const override;
        std::tuple<HelloResponse, Error> greet(const HelloRequest&) override;
        drogon::Task<std::tuple<std::optional<ModResponse>, Error>> getMod(const ModRequest&) override;
    };
}
