#include "AdminFilter.h"

#include <drogon/drogon.h>
#include <service/auth.h>
#include <service/util.h>

using namespace drogon;
using namespace service;

Task<HttpResponsePtr> AdminFilter::doFilter(const HttpRequestPtr &req) {
    const auto session(co_await global::auth->getSession(req));

    if (const auto role = session.user.getValueOfRole(); role != ROLE_ADMIN) {
        co_return statusResponse(k403Forbidden);
    }

    co_return nullptr;
}
