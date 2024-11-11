#define GITHUB_API_URL "https://api.github.com/"

#include "github.h"
#include "log/log.h"

#include <drogon/drogon.h>

#include <chrono>
#include <fstream>
#include <jwt-cpp/jwt.h>
#include <map>

#include "util.h"

using namespace logging;
using namespace drogon;
using namespace std::chrono_literals;

bool isSuccess(const HttpStatusCode &code) { return code == k200OK || code == k201Created || code == k202Accepted; }

Task<std::optional<Json::Value>> sendApiRequest(HttpClientPtr client, HttpMethod method, std::string path,
                                                std::string token,
                                                const std::map<std::string, std::string> &params = {}) {
    try {
        auto httpReq = HttpRequest::newHttpRequest();
        httpReq->setMethod(method);
        httpReq->setPath(path);
        httpReq->addHeader("Accept", "application/vnd.github+json");
        httpReq->addHeader("Authorization", "Bearer " + token);
        httpReq->addHeader("X-GitHub-Api-Version", "2022-11-28");
        for (const auto &[key, val]: params) {
            httpReq->setParameter(key, val);
        }

        logger.debug("=> Request to {}", path);
        const auto response = co_await client->sendRequestCoro(httpReq);
        const auto status = response->getStatusCode();
        if (isSuccess(status)) {
            if (const auto jsonResp = response->getJsonObject()) {
                logger.trace("<= Response ({}) from {}", std::to_string(status), path);
                co_return std::make_optional(*jsonResp);
            }
        }

        logger.error("Unexpected api response: ({}) {}", std::to_string(status), response->getBody());
        co_return std::nullopt;
    } catch (std::exception &e) {
        logger.error(e.what());
        co_return std::nullopt;
    }
}

HttpClientPtr createHttpClient() {
    auto currentLoop = trantor::EventLoop::getEventLoopOfCurrentThread();
    auto client = HttpClient::newHttpClient(GITHUB_API_URL, currentLoop);
    return client;
}

std::optional<std::chrono::system_clock::time_point> parseISO8601(const std::string &iso_time) {
    std::istringstream ss(iso_time);
    std::chrono::system_clock::time_point tp;
    ss >> std::chrono::parse("%Y-%m-%dT%H:%M:%SZ", tp);

    if (ss.fail()) {
        logger.error("Not a valid ISO 8601 string: {}", iso_time);
        return std::nullopt;
    }
    return std::optional{tp};
}

namespace service {
    GitHub::GitHub(service::MemoryCache &cache_, const std::string &appClientId_,
                   const std::string &appPrivateKeyPath_) :
        cache_(cache_), appClientId_(appClientId_), appPrivateKeyPath_(appPrivateKeyPath_) {}

    Task<std::tuple<std::optional<std::string>, Error>> GitHub::getApplicationJWTToken() {
        if (const auto cached = co_await cache_.getFromCache("jwt:" + appClientId_)) {
            logger.trace("Reusing cached app JWT token");
            co_return {cached, Error::Ok};
        }

        std::ifstream t(appPrivateKeyPath_);
        std::string contents((std::istreambuf_iterator(t)), std::istreambuf_iterator<char>());

        const auto issuedAt = std::chrono::system_clock::now() - 60s;
        const auto expiresAt = std::chrono::system_clock::now() + (10 * 60s);

        std::string token;
        try {
            token = jwt::create()
                            .set_issuer(appClientId_)
                            .set_issued_at(issuedAt)
                            .set_expires_at(expiresAt)
                            .sign(jwt::algorithm::rs256("", contents));

            // TODO don't block thread
            logger.trace("Storing JWT token in cache");
            const auto ttl =
                    std::chrono::duration_cast<std::chrono::seconds>(expiresAt - std::chrono::system_clock::now()) - 5s;
            co_await cache_.updateCache("jwt:" + appClientId_, token, ttl);
        } catch (std::exception &e) {
            logger.error("Failed to issue JWT auth token: {}", e.what());
            co_return {std::nullopt, Error::ErrInternal};
        }

        co_return {std::optional{token}, Error::Ok};
    }

    Task<std::tuple<std::optional<std::string>, Error>> GitHub::getRepositoryInstallation(const std::string repo) {
        if (const auto cached = co_await cache_.getFromCache("repo_install_id:" + repo)) {
            logger.trace("Reusing cached repo installation ID for {}", repo);
            co_return {cached, Error::Ok};
        }

        const auto [jwt, error](co_await getApplicationJWTToken());

        const auto client = createHttpClient();
        if (auto installation = co_await sendApiRequest(client, Get, std::format("/repos/{}/installation", repo), *jwt))
        {
            const std::string installationId = (*installation)["id"].asString();

            logger.trace("Storing installation ID for {} in cache", repo);
            co_await cache_.updateCache("repo_install_id:" + repo, installationId, 14 * 24h);

            co_return {std::optional{installationId}, Error::Ok};
        }

        logger.error("Failed to get repository installation on {}", repo);
        co_return {std::nullopt, Error::ErrNotFound};
    }

    Task<std::tuple<std::optional<std::string>, Error>> GitHub::getInstallationToken(const std::string installationId) {
        if (const auto cached = co_await cache_.getFromCache("installation_token:" + installationId)) {
            logger.trace("Reusing cached repo installation token for {}", installationId);
            co_return {cached, Error::Ok};
        }

        const auto [jwt, error](co_await getApplicationJWTToken());

        const auto client = createHttpClient();
        if (auto installationToken = co_await sendApiRequest(
                    client, Post, std::format("/app/installations/{}/access_tokens", installationId), *jwt))
        {
            const std::string expiryString = (*installationToken)["expires_at"].asString();
            const std::string token = (*installationToken)["token"].asString();

            if (const auto expiryTime = parseISO8601(expiryString)) {
                logger.trace("Storing installation token for {} in cache", installationId);
                const auto ttl = std::chrono::duration_cast<std::chrono::seconds>(*expiryTime -
                                                                                  std::chrono::system_clock::now()) -
                                 5s;
                co_await cache_.updateCache("installation_token:" + installationId, token, ttl);
            }

            co_return {std::optional{token}, Error::Ok};
        }

        logger.error("Failed to create installation token for {}", installationId);
        co_return {std::nullopt, Error::ErrInternal};
    }

    Task<std::tuple<std::optional<Json::Value>, Error>>
    GitHub::getRepositoryContents(const std::string repo, const std::optional<std::string> ref, const std::string path,
                                  const std::string installationToken) {
        const auto client = createHttpClient();
        std::map<std::string, std::string> params = {};
        if (ref) {
            params["ref"] = *ref;
        }

        const auto normalizedPath = removeTrailingSlash(path);
        const auto normalizedStart = path.starts_with('/') ? path.substr(1) : path;
        if (auto repositoryContents = co_await sendApiRequest(
                    client, Get, std::format("/repos/{}/contents/{}", repo, normalizedStart), installationToken, params))
        {
            co_return {std::optional(repositoryContents), Error::Ok};
        }

        logger.error("Failed to get repository contents, repo {} at {}", repo, path);
        co_return {std::nullopt, Error::ErrNotFound};
    }

    Task<std::tuple<bool, Error>> GitHub::isPublicRepository(std::string repo, std::string installationToken) {
        const auto client = createHttpClient();
        if (auto repositoryContents = co_await sendApiRequest(client, Get, "/repos/" + repo, installationToken)) {
            co_return {!(*repositoryContents)["private"].asBool(), Error::Ok};
        }
        co_return {false, Error::ErrNotFound};
    }

    Task<std::tuple<std::optional<std::string>, Error>> GitHub::getFileLastUpdateTime(std::string repo,
                                                                                      std::optional<std::string> ref,
                                                                                      std::string path,
                                                                                      std::string installationToken) {
        const auto client = createHttpClient();
        std::map<std::string, std::string> params = { {"page", "1"}, {"per_page", "1"} };
        params.try_emplace("path", path);
        if (ref) {
            params["sha"] = *ref;
        }
        if (auto commits =
                    co_await sendApiRequest(client, Get, std::format("/repos/{}/commits", repo), installationToken, params);
            commits && commits->isArray())
        {
            const auto entry = commits->front();
            if (entry.isMember("commit") && entry["commit"].isMember("committer") && entry["commit"]["committer"].isMember("date")) {
                const auto date = entry["commit"]["committer"]["date"].asString();
                co_return {date, Error::Ok};
            }
        }

        co_return {std::nullopt, Error::ErrNotFound};
    }
}
