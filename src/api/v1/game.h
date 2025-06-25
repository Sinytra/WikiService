#pragma once

#include <drogon/HttpController.h>

#include "base.h"

namespace api::v1 {
    class GameController final : public drogon::HttpController<GameController, false> {
    public:
        METHOD_LIST_BEGIN
        // Content
        ADD_METHOD_TO(GameController::contents, "/api/v1/content/{1:project}", drogon::Get, "AuthFilter");
        ADD_METHOD_TO(GameController::contentItem, "/api/v1/content/{1:project}/{2:id}", drogon::Get, "AuthFilter");
        ADD_METHOD_TO(GameController::contentItemRecipe, "/api/v1/content/{1:project}/{2:id}/recipe", drogon::Get, "AuthFilter");
        ADD_METHOD_TO(GameController::contentItemUsage, "/api/v1/content/{1:project}/{2:id}/usage", drogon::Get, "AuthFilter");
        // Recipes
        ADD_METHOD_TO(GameController::recipe, "/api/v1/content/{1:project}/recipe/{2:recipe}", drogon::Get, "AuthFilter");
        ADD_METHOD_TO(GameController::recipeType, "/api/v1/content/{1:project}/recipe-type/{2:recipe}", drogon::Get, "AuthFilter");
        METHOD_LIST_END

        drogon::Task<> contents(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback,
                                std::string project) const;
        drogon::Task<> contentItem(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback,
                                   std::string project, std::string id) const;
        drogon::Task<> contentItemRecipe(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback,
                                         std::string project, std::string item) const;
        drogon::Task<> contentItemUsage(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback,
                                        std::string project, std::string item) const;

        drogon::Task<> recipe(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback,
                              std::string project, std::string recipe) const;

        drogon::Task<> recipeType(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback,
                                  std::string project, std::string type) const;
    };
}
