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

std::string createRepoInstallIdCacheKey(const std::string &repo) { return "repo_install_id:" + repo; }

std::optional<std::chrono::system_clock::time_point> parseISO8601(const std::string &iso_time) {
    std::istringstream ss(iso_time);
    std::tm tm = {};

    // Parse the ISO 8601 date-time string into a tm struct.
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");

    if (ss.fail()) {
        logger.error("Not a valid ISO 8601 string: {}", iso_time);
        return std::nullopt;
    }

    // Convert tm to time_t (seconds since epoch)
    const std::time_t time_since_epoch = std::mktime(&tm) - timezone;

    // Convert time_t to system_clock::time_point
    return std::chrono::system_clock::from_time_t(time_since_epoch);
}

HttpRequestPtr newGithubApiRequest(const HttpMethod method, const std::string &path) {
    const auto req = HttpRequest::newHttpRequest();
    req->setMethod(method);
    req->setPath(path);
    req->addHeader("Accept", "application/vnd.github+json");
    req->addHeader("X-GitHub-Api-Version", "2022-11-28");
    return req;
}

namespace service {
    GitHub::GitHub(MemoryCache &cache_, const std::string &appName_, const std::string &appClientId_,
                   const std::string &appPrivateKeyPath_) :
        cache_(cache_), appName_(appName_), appClientId_(appClientId_), appPrivateKeyPath_(appPrivateKeyPath_) {}

    Task<std::tuple<std::optional<std::string>, Error>> GitHub::getUsername(std::string token) const {
        const auto client = createHttpClient(GITHUB_API_URL);
        if (auto user = co_await sendAuthenticatedRequest(client, Get, "/user", token); user && user->isObject() && user->isMember("login"))
        {
            const auto login = (*user)["login"].asString();
            co_return {login, Error::Ok};
        }
        co_return {std::nullopt, Error::Ok};
    }

    std::string GitHub::getAppInstallUrl() const { return std::format(GITHUB_APP_INSTALL_BASE_URL, appName_); }

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

    Task<std::tuple<std::optional<std::string>, Error>> GitHub::getRepositoryInstallation(const std::string repo) const {
        const auto key = createRepoInstallIdCacheKey(repo);

        if (const auto cached = co_await cache_.getFromCache(key)) {
            logger.trace("Reusing cached repo installation ID for {}", repo);
            co_return {cached, Error::Ok};
        }

        const auto [jwt, error](co_await getApplicationJWTToken());

        const auto client = createHttpClient(GITHUB_API_URL);
        if (auto installation = co_await sendAuthenticatedRequest(client, Get, std::format("/repos/{}/installation", repo), *jwt)) {
            const std::string installationId = (*installation)["id"].asString();

            logger.trace("Storing installation ID for {} in cache", repo);
            co_await cache_.updateCache(key, installationId, 14 * 24h);

            co_return {std::optional{installationId}, Error::Ok};
        }

        logger.error("Failed to get repository installation on {}", repo);
        co_return {std::nullopt, Error::ErrNotFound};
    }

    Task<std::tuple<std::set<std::string>, Error>> GitHub::getInstallationAccessibleRepos(const std::string installationId,
                                                                                          const std::string token) const {
        const auto client = createHttpClient(GITHUB_API_URL);

        // TODO Expand pagination
        if (const auto data =
                co_await sendAuthenticatedRequest(client, Get, std::format("/user/installations/{}/repositories", installationId), token);
            data && data->isObject() && data->isMember("repositories") && (*data)["repositories"].isArray())
        {
            std::set<std::string> set;
            for (const auto repos = (*data)["repositories"]; const auto &repo: repos) {
                if (const auto permissions = repo["permissions"]; permissions["admin"].asBool() || permissions["maintain"].asBool()) {
                    set.insert(repo["full_name"].asString());
                }
            }

            co_return {set, Error::Ok};
        }

        co_return {std::set<std::string>(), Error::Ok};
    }

    Task<std::tuple<Json::Value, Error>> GitHub::computeUserAccessibleInstallations(std::string token) const {
        const auto client = createHttpClient(GITHUB_API_URL);
        if (auto data = co_await sendAuthenticatedRequest(client, Get, "/user/installations", token);
            data && data->isObject() && data->isMember("installations") && (*data)["installations"].isArray())
        {
            const auto installations = (*data)["installations"];
            Json::Value result(Json::arrayValue);
            for (const auto &item: installations) {
                result.append(item["id"].asString());
            }
            co_return {result, Error::Ok};
        }
        co_return {Json::Value(Json::arrayValue), Error::ErrNotFound};
    }

    Task<std::set<std::string>> GitHub::getUserAccessibleInstallations(std::string token) {
        const auto [username, usernameErr] = co_await getUsername(token);
        if (!username) {
            co_return std::set<std::string>();
        }

        const auto cacheKey = "user_installations:" + *username;

        if (const auto cached = co_await cache_.getFromCache(cacheKey)) {
            if (const auto parsed = parseJsonString(*cached)) {
                std::set<std::string> set;
                for (const auto &item: *parsed) {
                    set.insert(item.asString());
                }
                co_return set;
            }
        }

        if (const auto pending = co_await getOrStartTask<std::set<std::string>>(cacheKey)) {
            co_return pending->get();
        }

        const auto [installations, installationsError] = co_await computeUserAccessibleInstallations(token);
        co_await cache_.updateCache(cacheKey, serializeJsonString(installations), 7 * 24h);

        std::set<std::string> set;
        for (const auto &item: installations) {
            set.insert(item.asString());
        }

        co_return co_await completeTask<std::set<std::string>>(cacheKey, std::move(set));
    }

    Task<std::tuple<std::optional<std::string>, Error>> GitHub::getInstallationToken(const std::string installationId) const {
        if (const auto cached = co_await cache_.getFromCache("installation_token:" + installationId)) {
            logger.trace("Reusing cached repo installation token for {}", installationId);
            co_return {cached, Error::Ok};
        }

        const auto [jwt, error](co_await getApplicationJWTToken());

        const auto client = createHttpClient(GITHUB_API_URL);
        if (auto installationToken =
                co_await sendAuthenticatedRequest(client, Post, std::format("/app/installations/{}/access_tokens", installationId), *jwt))
        {
            const std::string expiryString = (*installationToken)["expires_at"].asString();
            const std::string token = (*installationToken)["token"].asString();

            if (const auto expiryTime = parseISO8601(expiryString)) {
                logger.trace("Storing installation token for {} in cache", installationId);
                const auto ttl = std::chrono::duration_cast<std::chrono::seconds>(*expiryTime - std::chrono::system_clock::now()) - 5s;
                co_await cache_.updateCache("installation_token:" + installationId, token, ttl);
            }

            co_return {std::optional{token}, Error::Ok};
        }

        logger.error("Failed to create installation token for {}", installationId);
        co_return {std::nullopt, Error::ErrInternal};
    }

    Task<std::tuple<std::optional<std::string>, Error>> GitHub::getCollaboratorPermissionLevel(std::string repo, std::string username,
                                                                                               std::string installationToken) const {
        const auto client = createHttpClient(GITHUB_API_URL);
        if (auto permissions = co_await sendAuthenticatedRequest(
                client, Get, std::format("/repos/{}/collaborators/{}/permission", repo, username), installationToken);
            permissions && permissions->isObject() && permissions->isMember("permission"))
        {
            const auto perms = (*permissions)["permission"].asString();
            co_return {perms, Error::Ok};
        }
        co_return {std::nullopt, Error::Ok};
    }

    Task<std::tuple<std::vector<std::string>, Error>> GitHub::getRepositoryBranches(std::string repo, std::string installationToken) const {
        const auto client = createHttpClient(GITHUB_API_URL);
        std::vector<std::string> repoBranches;
        if (auto branches = co_await sendAuthenticatedRequest(client, Get, std::format("/repos/{}/branches", repo), installationToken);
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
        if (auto repositoryContents =
                co_await sendAuthenticatedRequest(client, Get, std::format("/repos/{}/contents/{}", repo, normalizedPath),
                                                  installationToken, [&ref](const HttpRequestPtr &req) { req->setParameter("ref", ref); }))
        {
            co_return {std::optional(repositoryContents), Error::Ok};
        }

        logger.error("Failed to get repository contents, repo {} at {}", repo, path);
        co_return {std::nullopt, Error::ErrNotFound};
    }

    Task<std::tuple<bool, Error>> GitHub::isPublicRepository(std::string repo, std::string installationToken) const {
        const auto client = createHttpClient(GITHUB_API_URL);
        if (auto repositoryContents = co_await sendAuthenticatedRequest(client, Get, "/repos/" + repo, installationToken)) {
            co_return {!(*repositoryContents)["private"].asBool(), Error::Ok};
        }
        co_return {false, Error::ErrNotFound};
    }

    Task<std::tuple<std::optional<std::string>, Error>> GitHub::getFileLastUpdateTime(std::string repo, std::string ref, std::string path,
                                                                                      std::string installationToken) const {
        const auto client = createHttpClient(GITHUB_API_URL);
        std::map<std::string, std::string> params = {{"page", "1"}, {"per_page", "1"}};
        params.try_emplace("path", path);
        params.try_emplace("sha", ref);
        if (auto commits = co_await sendAuthenticatedRequest(client, Get, std::format("/repos/{}/commits", repo), installationToken,
                                                             [&path, &ref](const HttpRequestPtr &req) {
                                                                 req->setParameter("page", "1");
                                                                 req->setParameter("per_page", "1");
                                                                 req->setParameter("path", path);
                                                                 req->setParameter("sha", ref);
                                                             });
            commits && commits->isArray())
        {
            if (const auto entry = commits->front();
                entry.isMember("commit") && entry["commit"].isMember("committer") && entry["commit"]["committer"].isMember("date"))
            {
                const auto date = entry["commit"]["committer"]["date"].asString();
                co_return {date, Error::Ok};
            }
        }

        co_return {std::nullopt, Error::ErrNotFound};
    }

    Task<> GitHub::invalidateCache(const std::string repo) const { co_await cache_.erase(createRepoInstallIdCacheKey(repo)); }

    Task<std::string> GitHub::getNewRepositoryLocation(std::string repo) const {
        const auto client = createHttpClient(GITHUB_API_URL);
        const auto path = std::format("/repos/{}", repo);
        try {
            const auto httpReq = newGithubApiRequest(Get, path);
            const auto response = co_await client->sendRequestCoro(httpReq);
            // Check if repo was moved
            if (const auto status = response->getStatusCode(); status == k301MovedPermanently) {
                if (const auto location = response->getHeader("location"); !location.empty()) {
                    // Send request to new location
                    const auto redirectClient = createHttpClient(location);
                    const auto redirectPath = location.substr((redirectClient->secure() ? 8 : 7) + redirectClient->getHost().size());

                    const auto redirectReq = newGithubApiRequest(Get, redirectPath);

                    if (const auto redirectResp = co_await client->sendRequestCoro(redirectReq); isSuccess(redirectResp->getStatusCode())) {
                        if (const auto jsonResp = redirectResp->getJsonObject(); jsonResp->isObject() && jsonResp->isMember("full_name")) {
                            co_return (*jsonResp)["full_name"].asString();
                        }
                    }
                }
            }
        } catch (std::exception &e) {
            logger.error(e.what());
        }

        co_return "";
    }
}
