#pragma once

#include <drogon/HttpTypes.h>
#include <drogon/HttpResponse.h>

#include <functional>

#include "service/error.h"

namespace api::v1 {
    drogon::HttpStatusCode mapError(const service::Error&);

    void errorResponse(const service::Error &error, const std::string &message, std::function<void(const drogon::HttpResponsePtr &)> &callback);

    void simpleError(
        const service::Error &error, const std::string &message, const std::function<void(const drogon::HttpResponsePtr &)> &callback,
        const std::function<void(Json::Value &)> &jsonBuilder = [](Json::Value &) {});
}
