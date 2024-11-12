#pragma once

#include <drogon/HttpTypes.h>

#include "service/error.h"

namespace api::v1 {
    drogon::HttpStatusCode mapError(const service::Error);
}
