#include "cors.h"

#include <drogon/HttpAppFramework.h>
#include <drogon/HttpResponse.h>
#include <drogon/drogon.h>

using namespace drogon;

void setupCors() {
    // Register sync advice to handle CORS preflight (OPTIONS) requests
    app().registerSyncAdvice([](const HttpRequestPtr &req) -> HttpResponsePtr {
        if (req->method() == Options) {
            auto resp = HttpResponse::newHttpResponse();

            // Set Access-Control-Allow-Origin header based on the Origin
            // request header
            const auto &origin = req->getHeader("Origin");
            if (!origin.empty()) {
                resp->addHeader("Access-Control-Allow-Origin", origin);
            }

            // Set Access-Control-Allow-Methods based on the requested method
            const auto &requestMethod = req->getHeader("Access-Control-Request-Method");
            if (!requestMethod.empty()) {
                resp->addHeader("Access-Control-Allow-Methods", requestMethod);
            }

            // Allow credentials to be included in cross-origin requests
            resp->addHeader("Access-Control-Allow-Credentials", "true");

            // Set allowed headers from the Access-Control-Request-Headers
            // header
            const auto &requestHeaders = req->getHeader("Access-Control-Request-Headers");
            if (!requestHeaders.empty()) {
                resp->addHeader("Access-Control-Allow-Headers", requestHeaders);
            }

            return std::move(resp);
        }
        return {};
    });

    // Register post-handling advice to add CORS headers to all responses
    app().registerPostHandlingAdvice([](const HttpRequestPtr &req, const HttpResponsePtr &resp) -> void {
        // Set Access-Control-Allow-Origin based on the Origin request
        // header
        const auto &origin = req->getHeader("Origin");
        if (!origin.empty()) {
            resp->addHeader("Access-Control-Allow-Origin", origin);
        }

        // Reflect the requested Access-Control-Request-Method back in the
        // response
        const auto &requestMethod = req->getHeader("Access-Control-Request-Method");
        if (!requestMethod.empty()) {
            resp->addHeader("Access-Control-Allow-Methods", requestMethod);
        }

        // Allow credentials to be included in cross-origin requests
        resp->addHeader("Access-Control-Allow-Credentials", "true");

        // Reflect the requested Access-Control-Request-Headers back
        const auto &requestHeaders = req->getHeader("Access-Control-Request-Headers");
        if (!requestHeaders.empty()) {
            resp->addHeader("Access-Control-Allow-Headers", requestHeaders);
        }
    });
}
