#include "system.h"
#include "error.h"

#include <log/log.h>
#include <models/Project.h>
#include <service/serializers.h>
#include <service/util.h>
#include <version.h>

#include "lang/crowdin.h"
#include "service/auth.h"
#include "service/system_data/system_data_import.h"

using namespace std;
using namespace drogon;
using namespace service;
using namespace logging;

using namespace drogon::orm;
using namespace drogon_model::postgres;

namespace api::v1 {
    Task<> SystemController::getLocales(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback) const {
        const auto locales = co_await global::crowdin->getAvailableLocales();
        callback(jsonResponse(locales));
    }

    Task<> SystemController::getSystemInformation(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback) const {
        co_await global::auth->ensurePrivilegedAccess(req);

        const auto imports{co_await global::database->getDataImports("", 1)};
        const auto latestImport = imports.size > 0 ? imports.data.front() : nlohmann::json{nullptr};
        const auto projectCount{co_await global::database->getTotalModelCount<Project>()};
        const auto userCount{co_await global::database->getTotalModelCount<User>()};

        nlohmann::json root;
        root["version"] = PROJECT_VERSION;
        root["version_hash"] = PROJECT_GIT_HASH;
        root["latest_data"] = latestImport;
        {
            nlohmann::json stats;
            stats["projects"] = projectCount;
            stats["users"] = userCount;
            root["stats"] = stats;
        }

        callback(jsonResponse(root));
    }

    Task<> SystemController::getDataImports(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback) const {
        co_await global::auth->ensurePrivilegedAccess(req);

        const auto [query, page] = getTableQueryParams(req);
        const auto imports{co_await global::database->getDataImports(query, page)};
        callback(jsonResponse(imports));
    }

    Task<> SystemController::importData(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback) const {
        co_await global::auth->ensurePrivilegedAccess(req);

        const auto json(req->getJsonObject());
        if (!json) {
            throw ApiException(Error::ErrBadRequest, req->getJsonError());
        }
        const auto parsed = parkourJson(*json);

        constexpr sys::SystemDataImporter importer;
        const auto result = co_await importer.importData(parsed);

        callback(statusResponse(result == Error::Ok ? k200OK : k500InternalServerError));

        co_return;
    }
}
