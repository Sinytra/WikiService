#pragma once

#include "error.h"
#include "models/Mod.h"

#include <drogon/utils/coroutine.h>
#include <optional>
#include <string>

using namespace drogon_model::postgres;

namespace service {
    class Database {
    public:
        explicit Database();

        drogon::Task<std::tuple<std::optional<Mod>, Error>> getModSource(std::string id);
    };
}
