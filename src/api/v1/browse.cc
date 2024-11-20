#include "browse.h"

#include <models/Project.h>

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
    BrowseController::BrowseController(Database &db) : database_(db) {}

    Task<> BrowseController::browse(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback, std::string query, int page) const {
        const auto [searchResults, searchError] = co_await database_.findProjects(query, page);

        Json::Value root;
        Json::Value data(Json::arrayValue);
        for (const auto &item: searchResults.data) {
            data.append(item.toJson());
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
