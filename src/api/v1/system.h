#pragma once

#include <drogon/HttpController.h>

#include <service/database/database.h>

namespace api::v1 {
    class SystemController final : public drogon::HttpController<SystemController, false> {
    public:
        METHOD_LIST_BEGIN
        ADD_METHOD_TO(SystemController::getSystemInformation, "/api/v1/system/info", drogon::Get, "AuthFilter");
        ADD_METHOD_TO(SystemController::getDataImports, "/api/v1/system/imports", drogon::Get, "AuthFilter");
        ADD_METHOD_TO(SystemController::importData, "/api/v1/system/import", drogon::Post, "AuthFilter");
        METHOD_LIST_END

        drogon::Task<> getSystemInformation(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback) const;

        drogon::Task<> getDataImports(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback) const;
        drogon::Task<> importData(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback) const;
    };
}
