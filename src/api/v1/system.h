#pragma once

#include <drogon/HttpController.h>

#include <service/database/database.h>

namespace api::v1 {
    class SystemController final : public drogon::HttpController<SystemController, false> {
    public:
        METHOD_LIST_BEGIN
        // Public
        ADD_METHOD_TO(SystemController::getLocales, "/api/v1/system/locales", drogon::Get, "AuthFilter");
        ADD_METHOD_TO(SystemController::importData, "/api/v1/system/import", drogon::Post);
        // Internal
        ADD_METHOD_TO(SystemController::getSystemInformation, "/api/v1/system/info", drogon::Get, "AuthFilter");
        ADD_METHOD_TO(SystemController::getDataImports, "/api/v1/system/imports", drogon::Get, "AuthFilter");
        ADD_METHOD_TO(SystemController::getAvailableMigrations, "/api/v1/system/migrations", drogon::Get, "AuthFilter");
        ADD_METHOD_TO(SystemController::runDataMigration, "/api/v1/system/migrate/{1:id}", drogon::Post, "AuthFilter");
        // Access Keys
        ADD_METHOD_TO(SystemController::getAccessKeys, "/api/v1/system/keys", drogon::Get, "AuthFilter");
        ADD_METHOD_TO(SystemController::createAccessKey, "/api/v1/system/keys", drogon::Post, "AuthFilter");
        ADD_METHOD_TO(SystemController::deleteAccessKey, "/api/v1/system/keys/{1:id}", drogon::Delete, "AuthFilter");
        METHOD_LIST_END

        drogon::Task<> getLocales(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback) const;

        drogon::Task<> getSystemInformation(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback) const;

        drogon::Task<> getDataImports(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback) const;
        drogon::Task<> importData(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback) const;

        drogon::Task<> getAvailableMigrations(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback) const;
        drogon::Task<> runDataMigration(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback, std::string id) const;

        drogon::Task<> getAccessKeys(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback) const;
        drogon::Task<> createAccessKey(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback) const;
        drogon::Task<> deleteAccessKey(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback, int64_t id) const;
    private:
        drogon::Task<> executeDataMigration(std::string id, std::function<drogon::Task<service::Error>()> runner) const;
    };
}
