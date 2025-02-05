#pragma once

#include <drogon/HttpController.h>

#include "service/database.h"
#include "service/storage.h"

namespace api::v1 {
    class GameController final : public drogon::HttpController<GameController, false> {
    public:
        explicit GameController(Database &, Storage &);

        METHOD_LIST_BEGIN
        // Recipes
        ADD_METHOD_TO(GameController::recipe, "/api/v1/content/{1:project}/recipe/{2:recipe}", drogon::Get, "AuthFilter");
        // Items
        ADD_METHOD_TO(GameController::itemUsage, "/api/v1/content/{1:project}/item/{2:item}/usage", drogon::Get, "AuthFilter");
        METHOD_LIST_END

        drogon::Task<> recipe(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback,
                               std::string project, std::string recipe) const;
        drogon::Task<> itemUsage(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback,
                               std::string project, std::string item) const;

    private:
        Database &database_;
        Storage &storage_;
    };
}