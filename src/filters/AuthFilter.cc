#include "AuthFilter.h"

#include <drogon/drogon.h>

using namespace drogon;

void AuthFilter::doFilter(const HttpRequestPtr &req, FilterCallback &&fcb, FilterChainCallback &&fccb) {
    const auto config = app().getCustomConfig();
    if (config.isMember("api_key") && !config["api_key"].asString().empty()) {
        const auto token = req->getHeader("Authorization");
        if (!token.empty() && config["api_key"] == token.substr(7)) {
            fccb();
            return;
        }
        const auto res = drogon::HttpResponse::newHttpResponse();
        res->setStatusCode(k401Unauthorized);
        fcb(res);
    } else {
        fccb();
    }
}
