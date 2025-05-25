#include "system.h"
#include "error.h"

#include <log/log.h>
#include <models/Project.h>
#include <service/util.h>

#include "service/auth.h"
#include "service/system_data/system_data_import.h"

#define ROLE_ADMIN "admin"

using namespace std;
using namespace drogon;
using namespace service;
using namespace logging;

using namespace drogon::orm;
using namespace drogon_model::postgres;

namespace api::v1 {
    Task<> ensurePrivilegedAccess(const HttpRequestPtr req) {
        const auto session(co_await global::auth->getSession(req));

        if (const auto role = session.user.getValueOfRole(); role != ROLE_ADMIN) {
            throw ApiException(Error::ErrForbidden, "forbidden");
        }
    }

    Task<> SystemController::importData(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback) const {
        co_await ensurePrivilegedAccess(req);

        const auto json(req->getJsonObject());
        if (!json) {
            throw ApiException(Error::ErrBadRequest, req->getJsonError());
        }
        const auto parsed = parkourJson(*json);

        const sys::SystemDataImporter importer;
        const auto result = co_await importer.importData(parsed);

        const auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(result == Error::Ok ? k200OK : k500InternalServerError);
        callback(resp);

        co_return;
    }
}
