#pragma once

#include "error.h"
#include "models/Mod.h"

#include <drogon/utils/coroutine.h>
#include <optional>
#include <string>

using namespace drogon_model::postgres;

namespace service {
    struct BrowseModsResponse {
        int pages;
        int total;
        std::vector<Mod> data;
    };

    class Database {
    public:
        explicit Database();

        drogon::Task<std::tuple<std::optional<Mod>, Error>> getModSource(std::string id);

        drogon::Task<std::tuple<BrowseModsResponse, Error>> findMods(std::string query, int page);
    };
}
