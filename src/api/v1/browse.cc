#include "browse.h"

#include <models/Mod.h>

#include "log/log.h"

#include <fstream>
#include <string>

#include "error.h"

using namespace std;
using namespace drogon;
using namespace service;
using namespace logging;

using namespace drogon::orm;
using namespace drogon_model::postgres;

namespace api::v1 {
    Json::Value modToJson(const Mod& mod) {
        Json::Value json;
        json["id"] = mod.getValueOfId();
        json["name"] = mod.getValueOfName();
        json["platform"] = mod.getValueOfPlatform();
        json["slug"] = mod.getValueOfSlug();
        json["source_repo"] = mod.getValueOfSourceRepo();
        json["is_community"] = mod.getValueOfIsCommunity();
        return json;
    }

    BrowseController::BrowseController(Database &db) : database_(db) {}

    Task<> BrowseController::browse(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback, std::string query, int page) const {
        const auto [searchResults, searchError] = co_await database_.findMods(query, page);

        Json::Value root;
        Json::Value data(Json::arrayValue);
        for (const auto &item: searchResults.data) {
            data.append(modToJson(item));
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
