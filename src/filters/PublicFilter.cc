#include "PublicFilter.h"

#include <drogon/drogon.h>

using namespace drogon;

void PublicFilter::doFilter(const HttpRequestPtr &req, FilterCallback &&fcb, FilterChainCallback &&fccb) {
    if (!req->getOptionalParameter<std::string>("token")) {
        if (const auto cookieSession = req->getCookie("sessionid"); !cookieSession.empty()) {
            req->setParameter("token", cookieSession);
        }
    }

    // TODO Rate limits

    fccb();
}
