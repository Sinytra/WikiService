#include "platforms.h"

#define WIKI_USER_AGENT "Sinytra/modded-wiki/1.0.0"
#define MODRINTH_API_URL "https://api.modrinth.com/"
#define CURSEFORGE_API_URL "https://api.curseforge.com/"
#define MC_GAME_ID 432
#define MC_MODS_CAT 6

#include <drogon/drogon.h>

#include <utility>
#include "log/log.h"

using namespace logging;
using namespace drogon;

// TODO Common HTTP utils

bool isSuccessCode(const HttpStatusCode &code) { return code == k200OK || code == k201Created || code == k202Accepted; }

HttpClientPtr createHttpClientPlatforms(std::string baseUrl) {
    auto currentLoop = trantor::EventLoop::getEventLoopOfCurrentThread();
    auto client = HttpClient::newHttpClient(baseUrl, currentLoop);
    return client;
}

Task<std::optional<Json::Value>> sendPlatformApiRequest(HttpClientPtr client, HttpMethod method, std::string path, std::string token = "") {
    try {
        auto httpReq = HttpRequest::newHttpRequest();
        httpReq->setMethod(method);
        httpReq->setPath(path);
        httpReq->addHeader("User-Agent", WIKI_USER_AGENT);
        if (!token.empty()) {
            httpReq->addHeader("x-api-key", token);
        }

        logger.trace("=> Request to {}", path);
        const auto response = co_await client->sendRequestCoro(httpReq);
        const auto status = response->getStatusCode();
        if (isSuccessCode(status)) {
            if (const auto jsonResp = response->getJsonObject()) {
                logger.trace("<= Response ({}) from {}", std::to_string(status), path);
                co_return std::make_optional(*jsonResp);
            }
        }

        logger.trace("Unexpected api response: ({}) {}", std::to_string(status), response->getBody());
        co_return std::nullopt;
    } catch (std::exception &e) {
        logger.error(e.what());
        co_return std::nullopt;
    }
}

namespace service {
    drogon::Task<std::optional<PlatformProject>> ModrinthPlatform::getProject(std::string slug) {
        const auto client = createHttpClientPlatforms(MODRINTH_API_URL);
        if (const auto resp = co_await sendPlatformApiRequest(client, Get, "/v3/project/" + slug); resp && resp->isObject()) {
            const auto projectSlug = (*resp)["slug"].asString();
            const auto sourceUrl = resp->isMember("link_urls") && (*resp)["link_urls"].isMember("source")
                                       ? (*resp)["link_urls"]["source"]["url"].asString()
                                       : "";
            co_return PlatformProject{.slug = projectSlug, .sourceUrl = sourceUrl};
        }
        co_return std::nullopt;
    }

    CurseForgePlatform::CurseForgePlatform(std::string apiKey) : apiKey_(std::move(apiKey)) {}

    drogon::Task<std::optional<PlatformProject>> CurseForgePlatform::getProject(std::string slug) {
        const auto client = createHttpClientPlatforms(CURSEFORGE_API_URL);
        if (const auto resp = co_await sendPlatformApiRequest(
                client, Get, std::format("/v1/mods/search?gameId={}&classId={}&slug={}", MC_GAME_ID, MC_MODS_CAT, slug), apiKey_);
            resp && resp->isObject())
        {
            const auto pagination = (*resp)["pagination"];
            const auto resultCount = pagination["resultCount"].asInt();
            const auto data = (*resp)["data"];
            if (resultCount == 1 && data.size() == 1) {
                const auto &proj = data[0];
                const auto projectSlug = proj["slug"].asString();
                const auto sourceUrl = proj.isMember("links") && proj["links"].isMember("sourceUrl") ? proj["links"]["sourceUrl"].asString() : "";

                co_return PlatformProject{.slug = projectSlug, .sourceUrl = sourceUrl};
            }
        }
        co_return std::nullopt;
    }

    Platforms::Platforms(const std::map<std::string, DistributionPlatform &> &map) : platforms_(map) {}

    drogon::Task<std::optional<PlatformProject>> Platforms::getProject(std::string platform, std::string slug) {
        const auto plat = platforms_.find(platform);

        if (plat == platforms_.end()) {
            co_return std::nullopt;
        }

        co_return co_await plat->second.getProject(slug);
    }
}
