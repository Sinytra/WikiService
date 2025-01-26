#define GITHUB_API_URL "https://api.github.com/"
#define GITHUB_APP_INSTALL_BASE_URL "https://github.com/apps/{}/installations/new"

#include "github.h"
#include "log/log.h"
#include "util.h"

#include <drogon/drogon.h>

#include <chrono>
#include <fstream>
#include <jwt-cpp/jwt.h>
#include <map>

using namespace logging;
using namespace drogon;
using namespace std::chrono_literals;

std::string createAppJWTTokenCacheKey(const std::string &appClientId) { return "jwt:" + appClientId; }

namespace service {
    GitHub::GitHub(MemoryCache &cache_, const std::string &appName_, const std::string &appClientId_,
                   const std::string &appPrivateKeyPath_) :
        cache_(cache_), appName_(appName_), appClientId_(appClientId_), appPrivateKeyPath_(appPrivateKeyPath_) {}

    Task<std::tuple<std::optional<Json::Value>, Error>> GitHub::getAuthenticatedUser(const std::string token) const {
        const auto client = createHttpClient(GITHUB_API_URL);
        auto [user, err] = co_await sendAuthenticatedRequest(client, Get, "/user", token);
        if (user && user->isObject()) {
            co_return {*user, err};
        }
        co_return {std::nullopt, err};
    }

    Task<std::tuple<std::optional<std::string>, Error>> GitHub::getApplicationJWTToken() const {
        const auto key = createAppJWTTokenCacheKey(appClientId_);

        if (const auto cached = co_await cache_.getFromCache(key)) {
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

            logger.trace("Storing JWT token in cache");
            const auto ttl = std::chrono::duration_cast<std::chrono::seconds>(expiresAt - std::chrono::system_clock::now()) - 5s;
            co_await cache_.updateCache(key, token, ttl);
        } catch (std::exception &e) {
            logger.error("Failed to issue JWT auth token: {}", e.what());
            co_return {std::nullopt, Error::ErrInternal};
        }

        co_return {std::optional{token}, Error::Ok};
    }

    Task<std::tuple<std::vector<std::string>, Error>> GitHub::getRepositoryBranches(std::string repo, std::string installationToken) const {
        const auto client = createHttpClient(GITHUB_API_URL);
        std::vector<std::string> repoBranches;
        if (auto [branches, err] =
                co_await sendAuthenticatedRequest(client, Get, std::format("/repos/{}/branches", repo), installationToken);
            branches && branches->isArray())
        {
            for (const auto &item: *branches) {
                const auto name = item["name"].asString();
                repoBranches.push_back(name);
            }
        }
        co_return {repoBranches, Error::Ok};
    }

    Task<std::tuple<std::optional<Json::Value>, Error>> GitHub::getRepositoryJSONFile(std::string repo, std::string ref, std::string path,
                                                                                      std::string installationToken) const {
        const auto [contents, contentsError](co_await getRepositoryContents(repo, ref, path, installationToken));
        if (contents) {
            const auto body = decodeBase64((*contents)["content"].asString());
            if (const auto parsed = parseJsonString(body)) {
                co_return {parsed, Error::Ok};
            }
            co_return {std::nullopt, Error::ErrBadRequest};
        }
        co_return {std::nullopt, contentsError};
    }

    Task<std::tuple<std::optional<Json::Value>, Error>> GitHub::getRepositoryContents(const std::string repo, const std::string ref,
                                                                                      const std::string path,
                                                                                      const std::string installationToken) const {
        const auto client = createHttpClient(GITHUB_API_URL);
        std::map<std::string, std::string> params;
        params.try_emplace("ref", ref);

        const auto normalizedPath = removeLeadingSlash(removeTrailingSlash(path));
        if (auto [repositoryContents, err] =
                co_await sendAuthenticatedRequest(client, Get, std::format("/repos/{}/contents/{}", repo, normalizedPath),
                                                  installationToken, [&ref](const HttpRequestPtr &req) { req->setParameter("ref", ref); });
            repositoryContents)
        {
            co_return {std::optional(repositoryContents), Error::Ok};
        }

        logger.error("Failed to get repository contents, repo {} at {}", repo, path);
        co_return {std::nullopt, Error::ErrNotFound};
    }
}
