#include "users.h"
#include "error.h"
#include "util.h"

using namespace drogon;
using namespace std::chrono_literals;

namespace service {
    Users::Users(Database &d, GitHub &g, MemoryCache &c) : database_(d), github_(g), cache_(c) {}

    Task<std::tuple<std::optional<UserInfo>, Error>> Users::getUserInfo(const std::string token) const {
        if (token.empty()) {
            // errorResponse(Error::ErrBadRequest, "Missing token parameter", callback);
            co_return {std::nullopt, Error::ErrBadRequest};
        }

        const std::string hashed(computeSHA256Hash(token));
        if (const auto cached = co_await cache_.getFromCache(hashed)) {
            if (const auto parsed = parseJsonString(*cached)) {
                Json::Value cProfile = (*parsed)["profile"];
                Json::Value cRepositories = (*parsed)["repositories"];
                auto cProjectIDs = parkourJson((*parsed)["projects"]).get<std::set<std::string>>();
                co_return {UserInfo{.profile = cProfile, .repositories = cRepositories, .projects = cProjectIDs}, Error::Ok};
            }
        }

        const auto [user, usernameErr] = co_await github_.getAuthenticatedUser(token);
        if (!user) {
            // errorResponse(usernameErr, "Couldn't fetch GitHub profile", callback);
            co_return {std::nullopt, Error::ErrNotFound};
        }
        const auto username = (*user)["login"].asString();

        Json::Value projectRepos(Json::arrayValue);
        std::vector<std::string> candidateRepos;

        for (const auto [installations, installationsErr] = co_await github_.getUserAccessibleInstallations(username, token);
             const auto &id: installations)
        {
            if (installationsErr != Error::Ok) {
                // errorResponse(installationsErr, "Error fetching installations", callback);
                co_return {std::nullopt, Error::ErrNotFound};
            }

            for (const auto [repos, reposErr] = co_await github_.getInstallationAccessibleRepos(id, token); const auto &repo: repos) {
                projectRepos.append(repo);
                candidateRepos.push_back(repo);
            }
        }

        // TODO Include community projects
        const auto projects = co_await database_.getProjectsForRepos(candidateRepos);

        std::set<std::string> projectIDs;
        for (auto &proj: projects) {
            projectIDs.insert(proj.getValueOfId());
        }

        Json::Value profile;
        profile["name"] = (*user)["name"].asString();
        profile["bio"] = (*user)["bio"].asString();
        profile["avatar_url"] = (*user)["avatar_url"].asString();
        profile["login"] = (*user)["login"].asString();
        profile["email"] = (*user)["email"].asString();

        nlohmann::json json;
        json["profile"] = parkourJson(profile);
        json["repositories"] = parkourJson(projectRepos);
        json["projects"] = projectIDs;

        co_await cache_.updateCache(hashed, json.dump(), 2h); // TODO unreliable cache time, handle OAuth on BE

        co_return {UserInfo{.profile = profile, .repositories = projectRepos, .projects = projectIDs}, Error::Ok};
    }

    Task<std::optional<UserInfo>> Users::getExistingUserInfo(std::string token) const {
        const std::string hashed = token;
        if (const auto cached = co_await cache_.getFromCache(hashed)) {
            if (const auto parsed = parseJsonString(*cached)) {
                Json::Value cProfile = (*parsed)["profile"];
                Json::Value cRepositories = (*parsed)["repositories"];
                auto cProjectIDs = parkourJson((*parsed)["projects"]).get<std::set<std::string>>();
                co_return UserInfo{.profile = cProfile, .repositories = cRepositories, .projects = cProjectIDs};
            }
        }
        co_return std::nullopt;
    }

    Task<Error> Users::refreshUserInfo(std::string username, std::string token) const {
        const std::string hashed(computeSHA256Hash(token));
        co_await github_.invalidateUserInstallations(username);
        co_await cache_.erase(hashed);

        const auto [info, error] = co_await getUserInfo(token);
        co_return error;
    }
}
