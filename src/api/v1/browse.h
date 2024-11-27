#pragma once

#include <drogon/HttpController.h>

#include "service/database.h"

namespace api::v1 {
    class BrowseController final : public drogon::HttpController<BrowseController, false> {
    public:
        explicit BrowseController(service::Database &);

        METHOD_LIST_BEGIN
        ADD_METHOD_TO(BrowseController::browse, "/api/v1/browse?query={1:query}&types={2:types}&sort={3:sort}&page={4:page}", drogon::Get, "AuthFilter");
        METHOD_LIST_END

        drogon::Task<> browse(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback, std::string query,
                              std::string types, std::string sort, int page) const;

    private:
        service::Database &database_;
    };
}
