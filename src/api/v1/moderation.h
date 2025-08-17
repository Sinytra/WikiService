#pragma once

#include <drogon/HttpController.h>

#include <service/database/database.h>

namespace api::v1 {
    class ModerationController final : public drogon::HttpController<ModerationController, false> {
    public:
        METHOD_LIST_BEGIN
        ADD_METHOD_TO(ModerationController::submitReport, "/api/v1/moderation/reports", drogon::Post, "AuthFilter");
        ADD_METHOD_TO(ModerationController::reports, "/api/v1/moderation/reports", drogon::Get, "AuthFilter");
        ADD_METHOD_TO(ModerationController::getReport, "/api/v1/moderation/reports/{1:id}", drogon::Get, "AuthFilter");
        ADD_METHOD_TO(ModerationController::ruleReport, "/api/v1/moderation/reports/{1:id}", drogon::Post, "AuthFilter");
        METHOD_LIST_END

        drogon::Task<> reports(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback) const;
        drogon::Task<> submitReport(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback) const;
        drogon::Task<> getReport(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback,
                                 std::string id) const;
        drogon::Task<> ruleReport(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback,
                                 std::string id) const;
    };
}
