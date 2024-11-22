#include <unordered_map>

#include "error.h"

using namespace std;
using namespace drogon;
using namespace service;

namespace {
    const unordered_map<Error, HttpStatusCode> errorMap = {
        {Error::Ok, k200OK}, {Error::ErrNotFound, k404NotFound}, {Error::ErrBadRequest, k400BadRequest}};
}

namespace api::v1 {
    HttpStatusCode mapError(const Error &err) {
        if (const auto cit(errorMap.find(err)); cit != errorMap.cend()) {
            return cit->second;
        }
        return k500InternalServerError;
    }

    void errorResponse(const Error &error, const std::string &message, std::function<void(const HttpResponsePtr &)> &callback) {
        Json::Value json;
        json["error"] = message;
        const auto resp = HttpResponse::newHttpJsonResponse(std::move(json));
        resp->setStatusCode(mapError(error));

        callback(resp);
    }

    void simpleError(const Error &error, const std::string &message, const std::function<void(const HttpResponsePtr &)> &callback,
                     const std::function<void(Json::Value &)> &jsonBuilder) {
        Json::Value root;
        root["error"] = message;
        jsonBuilder(root);
        const auto resp = HttpResponse::newHttpJsonResponse(root);
        resp->setStatusCode(mapError(error));
        callback(resp);
    }
}
