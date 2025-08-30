#include "frontend.h"

#include "api/v1/error.h"
#include "util.h"

#define REVALIDATE_API_PATH "/api/docs/{}/revalidate"

using namespace drogon;
using namespace logging;

namespace service {
    FrontendService::FrontendService(const std::string &frontendUrl, const std::string &apiKey) :
        frontendUrl_(frontendUrl), apiKey_(apiKey) {}

    Task<Error> FrontendService::revalidateProject(const std::string id) const {
        logger.debug("Revalidating frontend project '{}'", id);

        const auto client = createHttpClient(frontendUrl_);
        const auto httpReq = HttpRequest::newHttpRequest();
        httpReq->setMethod(Post);
        httpReq->setPath(std::format(REVALIDATE_API_PATH, id));
        httpReq->addHeader("Authorization", "Bearer " + apiKey_);

        const auto response = co_await client->sendRequestCoro(httpReq);

        if (response->getStatusCode() != k200OK) {
            const auto error = api::v1::mapStatusCode(response->getStatusCode());
            logger.error("Error revalidating frontend project: Unexpected response code: {}", std::to_string(response->getStatusCode()));
            co_return error;
        }

        co_return Error::Ok;
    }
}
