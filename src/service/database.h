#pragma once

#include "error.h"
#include "models/Mod.h"

#include <optional>
#include <string>
#include <drogon/utils/coroutine.h>

using namespace drogon_model::postgres;

namespace service
{
    class Database
    {
    public:
        explicit Database();

        drogon::Task<std::tuple<std::optional<Mod>, Error>> getModSource(std::string id);
    };
}
