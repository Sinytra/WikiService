#include "AuthFilter.h"

#include <drogon/drogon.h>

using namespace drogon;

void AuthFilter::doFilter(const HttpRequestPtr &req, FilterCallback &&fcb, FilterChainCallback &&fccb) {
    if (!req->getOptionalParameter<std::string>("token")) {
        if (const auto cookieSession = req->getCookie("sessionid"); !cookieSession.empty()) {
            req->setParameter("token", cookieSession);
        }
    }

    if (const auto config = app().getCustomConfig(); config.isMember("api_key") && !config["api_key"].asString().empty()) {
        if (const auto token = req->getHeader("Authorization"); !token.empty() && config["api_key"] == token.substr(7)) {
            fccb();
            return;
        }
        const auto res = HttpResponse::newHttpResponse();
        res->setStatusCode(k401Unauthorized);
        fcb(res);
    } else {
        fccb();
    }
}
