#include <unordered_map>

#include "error.h"

using namespace std;
using namespace drogon;
using namespace service;

namespace {
    const unordered_map<Error, HttpStatusCode> errorMap = {{Error::Ok, k200OK},
                                                           {Error::ErrNotFound, k404NotFound},
                                                           {Error::ErrBadRequest, k400BadRequest},
                                                           {Error::ErrUnauthorized, k401Unauthorized}};
}

namespace api::v1 {
    HttpStatusCode mapError(const Error &err) {
        if (const auto cit(errorMap.find(err)); cit != errorMap.cend()) {
            return cit->second;
        }
        return k500InternalServerError;
    }

    Error mapStatusCode(const HttpStatusCode& code) {
        for (const auto &[error, statusCode] : errorMap) {
            if (code == statusCode) {
                return error;
            }
        }
        return Error::ErrInternal;
    }
}
