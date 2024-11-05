#pragma once

#include <string>
#include <optional>
#include <tuple>

#include <drogon/utils/coroutine.h>
#include "error.h"

namespace service
{
    struct ModRequest
    {
        std::string id;
    };

    struct ModResponse
    {
        std::string id;
        std::string name;
    };

    struct HelloRequest
    {
        std::string greeting;
    };

    struct HelloResponse
    {
        std::string response;
    };

    class Service
    {
    public:
        virtual Error status() const = 0;
        virtual std::tuple<HelloResponse, Error> greet(const HelloRequest&) = 0;
        virtual drogon::Task<std::tuple<std::optional<ModResponse>, Error>> getMod(const ModRequest&) = 0;
    };
}
