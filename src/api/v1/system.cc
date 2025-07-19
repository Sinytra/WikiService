#include "system.h"
#include "error.h"

#include <log/log.h>
#include <models/Project.h>
#include <service/serializers.h>
#include <service/util.h>
#include <version.h>

#include "lang/crowdin.h"
#include "lang/lang.h"
#include "service/auth.h"
#include "service/system_data/system_data_import.h"

using namespace std;
using namespace drogon;
using namespace service;
using namespace logging;

using namespace drogon::orm;
using namespace drogon_model::postgres;

namespace api::v1 {
    struct DataMigration {
        std::string id;
        std::string title;
        std::string desc;
        std::function<Task<Error>()> runner;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(DataMigration, id, title, desc)
    };

    std::vector<DataMigration> dataMigrations = {
        {"deployments", "Project deployments", "Run initial project deployments", sys::runInitialDeployments}};

    Task<> SystemController::getLocales(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback) const {
        auto locales = co_await global::crowdin->getAvailableLocales();
        std::ranges::sort(locales, [](const Locale &a, const Locale &b) { return a.id < b.id; });
        locales.insert(locales.begin(), {"en", "English", DEFAULT_LOCALE});
        callback(jsonResponse(locales));
    }

    Task<> SystemController::getSystemInformation(const HttpRequestPtr req,
                                                  const std::function<void(const HttpResponsePtr &)> callback) const {
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
    }

    Task<> SystemController::getAvailableMigrations(const HttpRequestPtr req,
                                                    const std::function<void(const HttpResponsePtr &)> callback) const {
        co_await global::auth->ensurePrivilegedAccess(req);

        const nlohmann::json root = dataMigrations;

        callback(jsonResponse(root));
    }

    Task<> SystemController::runDataMigration(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                              const std::string id) const {
        co_await global::auth->ensurePrivilegedAccess(req);

        if (id.empty()) {
            throw ApiException(Error::ErrBadRequest, "Missing id parameter");
        }

        const auto migration = ranges::find_if(dataMigrations, [&id](const DataMigration &x) { return x.id == id; });
        if (migration == dataMigrations.end()) {
            throw ApiException(Error::ErrNotFound, "not_found");
        }

        app().getLoop()->queueInLoop(async_func([&]() -> Task<> {
            co_await executeDataMigration(id, migration->runner);
        }));

        callback(statusResponse(k200OK));
    }

    Task<> SystemController::executeDataMigration(const std::string id, const std::function<Task<Error>()> runner) const {
        try {
            co_await runner();
            logger.info("Data migration {} finished succesfully", id);
        } catch (std::exception e) {
            logger.error("Error running data migration {}: {}", id, e.what());
        }
    }
}
