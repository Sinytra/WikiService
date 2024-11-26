#include "browse.h"

#include <models/Project.h>

#include "log/log.h"

#include <string>

#include "error.h"

using namespace std;
using namespace drogon;
using namespace service;
using namespace logging;

using namespace drogon::orm;
using namespace drogon_model::postgres;

namespace api::v1 {
    BrowseController::BrowseController(Database &db) : database_(db) {}

    Task<> BrowseController::browse(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback, const std::string query,
                                    const std::string types, const int page) const {
        const auto [searchResults, searchError] = co_await database_.findProjects(query, types, page);

        Json::Value root;
        Json::Value data(Json::arrayValue);
        for (const auto &item: searchResults.data) {
            Json::Value itemJson;
            itemJson["id"] = item.getValueOfId();
            itemJson["name"] = item.getValueOfName();
            itemJson["type"] = item.getValueOfType();
            itemJson["platform"] = item.getValueOfPlatform();
            itemJson["slug"] = item.getValueOfSlug();
            itemJson["is_community"] = item.getValueOfIsCommunity();
            itemJson["created_at"] = item.getValueOfCreatedAt().toDbStringLocal();
            data.append(itemJson);
        }
        root["pages"] = searchResults.pages;
        root["total"] = searchResults.total;
        root["data"] = data;

        const auto resp = HttpResponse::newHttpJsonResponse(root);
        resp->setStatusCode(k200OK);
        callback(resp);

        co_return;
    }
}
