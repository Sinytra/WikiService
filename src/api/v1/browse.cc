#include "browse.h"
#include "error.h"

#include <string>

#include <global.h>
#include <log/log.h>
#include <models/Project.h>
#include <service/util.h>

using namespace std;
using namespace drogon;
using namespace service;
using namespace logging;

using namespace drogon::orm;
using namespace drogon_model::postgres;

namespace api::v1 {
    Task<> BrowseController::browse(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback, const std::string query,
                                    const std::string types, const std::string sort, const int page) const {
        const auto [searchResults, searchError] = co_await global::database->findProjects(query, types, sort, page);

        Json::Value root;
        Json::Value data(Json::arrayValue);
        for (const auto &item: searchResults.data) {
            Json::Value itemJson;
            itemJson["id"] = item.getValueOfId();
            itemJson["name"] = item.getValueOfName();
            itemJson["type"] = item.getValueOfType();
            itemJson["platforms"] = parseJsonString(item.getValueOfPlatforms()).value_or(Json::Value());
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
