#include "projects.h"

#include <log/log.h>
#include <models/Project.h>
#include <service/schemas.h>
#include <service/util.h>

#include "error.h"
#include "service/error.h"

using namespace std;
using namespace drogon;
using namespace service;
using namespace logging;

using namespace drogon::orm;
using namespace drogon_model::postgres;

namespace api::v1 {
    Task<bool> verifyProjectOwnership(const Platforms &pf, const PlatformProject &project, const std::string &repo,
                                      const std::string &mrCode) {
        if (const auto expected = "https://github.com/" + repo; !project.sourceUrl.empty() && project.sourceUrl.starts_with(expected)) {
            co_return true;
        }

        if (!mrCode.empty()) {
            if (const auto token = co_await pf.modrinth_.getOAuthToken(mrCode)) {
                if (const auto mrUsername = co_await pf.modrinth_.getAuthenticatedUser(*token)) {
                    co_return co_await pf.modrinth_.isProjectMember(project.slug, *mrUsername);
                }
            }
        }

        co_return false;
    }

    nlohmann::json processPlatforms(const std::vector<std::string> &valid, const Json::Value &metadata) {
        nlohmann::json projects(nlohmann::json::value_t::object);
        if (metadata.isMember("platforms") && metadata["platforms"].isObject()) {
            const auto platforms = metadata["platforms"];
            for (const auto &platform: valid) {
                if (platforms.isMember(platform) && platforms[platform].isString()) {
                    projects[platform] = platforms[platform].asString();
                }
            }
        }
        if (metadata.isMember("platform") && metadata["platform"].isString() && !projects.contains(metadata["platform"].asString()) &&
            metadata.isMember("slug") && metadata["slug"].isString())
        {
            projects[metadata["platform"].asString()] = metadata["slug"].asString();
        }
        return projects;
    }

    ProjectsController::ProjectsController(GitHub &gh, Platforms &pf, Database &db, Documentation &dc, CloudFlare &cf) :
        github_(gh), platforms_(pf), database_(db), documentation_(dc), cloudflare(cf) {}

    Task<std::optional<PlatformProject>> ProjectsController::validatePlatform(const std::string &id, const std::string &repo,
                                                                              const std::string &mrCode, const std::string &platform,
                                                                              const std::string &slug, const bool checkExisting,
                                                                              std::function<void(const HttpResponsePtr &)> callback) const {
        if (platform == PLATFORM_CURSEFORGE && !platforms_.curseforge_.isAvailable()) {
            simpleError(Error::ErrBadRequest, "cf_unavailable", callback);
            co_return std::nullopt;
        }

        const auto platformProj = co_await platforms_.getProject(platform, slug);
        if (!platformProj) {
            simpleError(Error::ErrBadRequest, "no_project", callback,
                        [&](Json::Value &root) { root["details"] = "Platform: " + platform; });
            co_return std::nullopt;
        }

        if (platformProj->type == _unknown) {
            simpleError(Error::ErrBadRequest, "unsupported_type", callback,
                        [&](Json::Value &root) { root["details"] = "Platform: " + platform; });
            co_return std::nullopt;
        }

        bool skipCheck = false;
        if (checkExisting) {
            if (const auto [proj, projErr] = co_await database_.getProjectSource(id); proj) {
                if (const auto platforms = parseJsonString(proj->getValueOfPlatforms());
                    platforms && platforms->isMember(platform) && (*platforms)[platform] == slug)
                {
                    skipCheck = true;
                }
            }
        }
        if (!skipCheck) {
            if (const auto verified = co_await verifyProjectOwnership(platforms_, *platformProj, repo, mrCode); !verified) {
                simpleError(Error::ErrBadRequest, "ownership", callback, [&](Json::Value &root) {
                    root["details"] = "Platform: " + platform;
                    root["can_verify_mr"] = platform == PLATFORM_MODRINTH && platforms_.modrinth_.isOAuthConfigured();
                });
                co_return std::nullopt;
            }
        }

        co_return platformProj;
    }

    /**
     * Possible errors:
     * - user_github_auth: Invalid GitHub auth token
     * - insufficient_wiki_perms: Insufficient admin perms (community projects)
     * - unsupported: Community project registration is not yet implemented
     * - missing_installation: App not installed on repository (data: install_url)
     * - insufficient_repo_perms: User is not admin/maintainer
     * - no_branch: Wrong branch specifiec
     * - no_meta: No metadata file
     * - invalid_meta: Metadata file format is invalid
     * - exists: Duplicate project ID
     * - no_platforms: No defined platform projects
     * - cf_unavailable: CF Project registration unavailable due to missing API key
     * - no_project: Platform project not found
     * - unsupported_type: Unsupported platform project type
     * - ownership: Failed to verify platform project ownership (data: can_verify_mr)
     * - internal: Internal server error
     */
    Task<std::optional<ValidatedProjectData>>
    ProjectsController::validateProjectData(const Json::Value &json, const std::string &token,
                                            std::function<void(const HttpResponsePtr &)> callback, const bool checkExisting) const {
        const auto [username, usernameError](co_await github_.getUsername(token));
        if (!username) {
            simpleError(Error::ErrBadRequest, "user_github_auth", callback);
            co_return std::nullopt;
        }

        // Required
        const auto branch = json["branch"].asString();
        const auto repo = json["repo"].asString();
        const auto path = json["path"].asString();
        // Optional
        const auto mrCode = json["mr_code"].asString();

        const auto [installationId, installationIdError](co_await github_.getRepositoryInstallation(repo));
        if (!installationId) {
            simpleError(Error::ErrBadRequest, "missing_installation", callback,
                        [&](Json::Value &root) { root["install_url"] = github_.getAppInstallUrl(); });
            co_return std::nullopt;
        }

        const auto [installationToken, tokenErr](co_await github_.getInstallationToken(installationId.value()));
        if (!installationToken) {
            simpleError(Error::ErrInternal, "internal", callback);
            co_return std::nullopt;
        }

        if (const auto [perms, permsError](co_await github_.getCollaboratorPermissionLevel(repo, *username, *installationToken));
            !perms || perms != "admin" && perms != "write")
        {
            simpleError(Error::ErrBadRequest, "insufficient_repo_perms", callback);
            co_return std::nullopt;
        }

        if (const auto [branches, branchesErr](co_await github_.getRepositoryBranches(repo, *installationToken));
            ranges::find(branches, branch) == branches.end())
        {
            simpleError(Error::ErrBadRequest, "no_branch", callback);
            co_return std::nullopt;
        }

        const auto metaFilePath = path + DOCS_META_FILE_PATH;
        const auto [contents, contentsError](co_await github_.getRepositoryJSONFile(repo, branch, metaFilePath, *installationToken));
        if (!contents) {
            simpleError(Error::ErrBadRequest, "no_meta", callback);
            co_return std::nullopt;
        }
        if (const auto error = validateJson(schemas::projectMetadata, *contents)) {
            simpleError(Error::ErrBadRequest, "invalid_meta", callback, [&error](Json::Value &root) { root["details"] = error->msg; });
            co_return std::nullopt;
        }
        const auto jsonContent = *contents;
        const auto id = jsonContent["id"].asString();

        const auto platforms = processPlatforms(platforms_.getAvailablePlatforms(), jsonContent);
        if (platforms.empty()) {
            simpleError(Error::ErrBadRequest, "no_platforms", callback);
            co_return std::nullopt;
        }

        std::unordered_map<std::string, PlatformProject> platformProjects;
        for (const auto &[key, val]: platforms.items()) {
            const auto platformProj = co_await validatePlatform(id, repo, mrCode, key, val, checkExisting, callback);
            if (!platformProj) {
                co_return std::nullopt;
            }
            platformProjects.emplace(key, *platformProj);
        }
        const auto &preferredProj =
            platforms.contains(PLATFORM_MODRINTH) ? platformProjects[PLATFORM_MODRINTH] : platformProjects.begin()->second;

        Project project;
        project.setId(id);
        project.setName(preferredProj.name);
        project.setSourceRepo(repo);
        project.setSourceBranch(branch);
        project.setSourcePath(path);
        project.setType(projectTypeToString(preferredProj.type));
        project.setPlatforms(platforms.dump());

        co_return ValidatedProjectData{*username, project, platforms};
    }

    Task<std::optional<Project>> ProjectsController::validateProjectAccess(const std::string &id, const std::string &token,
                                                                           std::function<void(const HttpResponsePtr &)> callback) const {
        if (id.empty()) {
            errorResponse(Error::ErrBadRequest, "Missing id route parameter", callback);
            co_return std::nullopt;
        }

        if (token.empty()) {
            errorResponse(Error::ErrBadRequest, "Missing token parameter", callback);
            co_return std::nullopt;
        }

        const auto [project, projectErr] = co_await database_.getProjectSource(id);
        if (!project) {
            simpleError(Error::ErrBadRequest, "not_found", callback);
            co_return std::nullopt;
        }
        const auto repo = project->getValueOfSourceRepo();

        if (const auto canAccess = co_await validateRepositoryAccess(repo, token, callback); !canAccess) {
            co_return std::nullopt;
        }

        co_return project;
    }

    Task<bool> ProjectsController::validateRepositoryAccess(const std::string &repo, const std::string &token,
                                                            std::function<void(const HttpResponsePtr &)> callback) const {
        const auto [username, usernameError](co_await github_.getUsername(token));
        if (!username) {
            simpleError(Error::ErrBadRequest, "user_github_auth", callback);
            co_return false;
        }

        const auto [installationId, installationIdError](co_await github_.getRepositoryInstallation(repo));
        if (!installationId) {
            simpleError(Error::ErrBadRequest, "missing_installation", callback);
            co_return false;
        }

        const auto [installationToken, tokenErr](co_await github_.getInstallationToken(installationId.value()));
        if (!installationToken) {
            simpleError(Error::ErrInternal, "internal", callback);
            co_return false;
        }

        if (const auto [perms, permsError](co_await github_.getCollaboratorPermissionLevel(repo, *username, *installationToken));
            !perms || perms != "admin" && perms != "write")
        {
            simpleError(Error::ErrBadRequest, "insufficient_repo_perms", callback);
            co_return false;
        }

        co_return true;
    }

    Task<> ProjectsController::greet(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) const {
        const auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k200OK);
        resp->setBody("Service operational");
        callback(resp);
        co_return;
    }

    Task<> ProjectsController::listIDs(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) const {
        const auto ids = co_await database_.getProjectIDs();

        Json::Value root(Json::arrayValue);
        for (const auto &id: ids) {
            root.append(id);
        }

        const auto resp = HttpResponse::newHttpJsonResponse(root);
        resp->setStatusCode(k200OK);
        callback(resp);
    }

    Task<> ProjectsController::listUserProjects(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback,
                                                const std::string token) const {
        if (token.empty()) {
            co_return errorResponse(Error::ErrBadRequest, "Missing token parameter", callback);
        }

        const auto [user, usernameErr] = co_await github_.getAuthenticatedUser(token);
        if (!user) {
            co_return errorResponse(usernameErr, "Couldn't fetch GitHub profile", callback);
        }
        const auto username = (*user)["login"].asString();

        Json::Value projectRepos(Json::arrayValue);
        std::vector<std::string> candidateRepos;

        // TODO Multithread
        for (const auto [installations, installationsErr] = co_await github_.getUserAccessibleInstallations(username, token);
             const auto &id: installations)
        {
            if (installationsErr != Error::Ok) {
                co_return errorResponse(installationsErr, "Error fetching installations", callback);
            }

            for (const auto [repos, reposErr] = co_await github_.getInstallationAccessibleRepos(id, token); const auto &repo: repos) {
                projectRepos.append(repo);
                candidateRepos.push_back(repo);
            }
        }

        // TODO Include community projects
        const auto projects = co_await database_.getProjectsForRepos(candidateRepos);
        Json::Value projectsJson(Json::arrayValue);
        for (const auto &project: projects) {
            projectsJson.append(projectToJson(project));
        }

        Json::Value root;
        {
            Json::Value profile;
            profile["name"] = (*user)["name"].asString();
            profile["bio"] = (*user)["bio"].asString();
            profile["avatar_url"] = (*user)["avatar_url"].asString();
            profile["login"] = (*user)["login"].asString();
            profile["email"] = (*user)["email"].asString();
            root["profile"] = profile;
        }
        root["repositories"] = projectRepos;
        root["projects"] = projectsJson;
        const auto resp = HttpResponse::newHttpJsonResponse(root);
        resp->setStatusCode(k200OK);
        callback(resp);
    }

    Task<> ProjectsController::listPopularProjects(HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback) const {
        const std::vector<std::string> ids = co_await cloudflare.getMostVisitedProjectIDs();
        Json::Value root(Json::arrayValue);
        for (const auto &id: ids) {
            if (const auto [project, error] = co_await database_.getProjectSource(id); project) {
                root.append(projectToJson(*project));
            }
        }
        const auto resp = HttpResponse::newHttpJsonResponse(root);
        resp->setStatusCode(k200OK);
        callback(resp);
    }

    Task<> ProjectsController::create(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback, std::string token) const {
        if (token.empty()) {
            co_return errorResponse(Error::ErrBadRequest, "Missing token parameter", callback);
        }

        auto json(req->getJsonObject());
        if (!json) {
            co_return errorResponse(Error::ErrBadRequest, req->getJsonError(), callback);
        }
        if (const auto error = validateJson(schemas::projectRegister, *json)) {
            co_return simpleError(Error::ErrBadRequest, error->msg, callback);
        }

        const auto isCommunity = (*json)["is_community"].asBool();

        // TODO Add support for community projects
        if (isCommunity) {
            co_return simpleError(Error::ErrInternal, "unsupported", callback);
        }

        const auto repo = (*json)["repo"].asString();
        const auto branch = (*json)["branch"].asString();
        const auto path = (*json)["path"].asString();

        if (co_await database_.existsForRepo(repo, branch, path)) {
            co_return simpleError(Error::ErrBadRequest, "exists", callback);
        }

        auto validated = co_await validateProjectData(*json, token, callback, false);
        if (!validated) {
            co_return;
        }
        auto [username, project, platforms] = *validated;
        project.setIsCommunity(isCommunity);

        if (co_await database_.existsForData(project.getValueOfId(), platforms)) {
            co_return simpleError(Error::ErrBadRequest, "exists", callback);
        }

        if (const auto [result, resultErr] = co_await database_.createProject(project); resultErr != Error::Ok) {
            logger.error("Failed to create project {} in database", project.getValueOfId());
            co_return simpleError(Error::ErrInternal, "internal", callback);
        }

        // Invalidate user installation cache to ensure project shows up on dev dashboard
        co_await github_.invalidateUserInstallations(username);

        Json::Value root;
        root["message"] = "Project registered successfully";
        const auto resp = HttpResponse::newHttpJsonResponse(root);
        resp->setStatusCode(k200OK);
        callback(resp);
    }

    Task<> ProjectsController::update(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback, std::string token) const {
        if (token.empty()) {
            co_return errorResponse(Error::ErrBadRequest, "Missing token parameter", callback);
        }

        const auto json(req->getJsonObject());
        if (!json) {
            co_return errorResponse(Error::ErrBadRequest, req->getJsonError(), callback);
        }
        if (const auto error = validateJson(schemas::projectRegister, *json)) {
            co_return simpleError(Error::ErrBadRequest, error->msg, callback);
        }

        auto validated = co_await validateProjectData(*json, token, callback, true);
        if (!validated) {
            co_return;
        }
        auto [username, project, platforms] = *validated;

        if (const auto [proj, projErr] = co_await database_.getProjectSource(project.getValueOfId()); !proj) {
            co_return simpleError(Error::ErrBadRequest, "not_found", callback);
        }

        if (const auto error = co_await database_.updateProject(project); error != Error::Ok) {
            logger.error("Failed to update project {} in database", project.getValueOfId());
            co_return simpleError(Error::ErrInternal, "internal", callback);
        }

        co_await github_.invalidateCache(project.getValueOfSourceRepo());
        co_await documentation_.invalidateCache(project);

        Json::Value root;
        root["message"] = "Project updated successfully";
        root["project"] = projectToJson(project);
        const auto resp = HttpResponse::newHttpJsonResponse(root);
        resp->setStatusCode(k200OK);
        callback(resp);
    }

    Task<> ProjectsController::remove(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback, const std::string id,
                                      const std::string token) const {
        const auto project = co_await validateProjectAccess(id, token, callback);
        if (!project) {
            co_return;
        }

        // Repo perms verified, proceed to deletion

        if (const auto error = co_await database_.removeProject(id); error != Error::Ok) {
            logger.error("Failed to delete project {} in database", id);
            co_return simpleError(Error::ErrInternal, "internal", callback);
        }

        co_await github_.invalidateCache(project->getValueOfSourceRepo());
        co_await documentation_.invalidateCache(*project);

        Json::Value root;
        root["message"] = "Project deleted successfully";
        const auto resp = HttpResponse::newHttpJsonResponse(root);
        resp->setStatusCode(k200OK);
        callback(resp);

        co_return;
    }

    Task<> ProjectsController::migrate(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback,
                                       const std::string token) const {
        if (token.empty()) {
            co_return errorResponse(Error::ErrBadRequest, "Missing token parameter", callback);
        }

        const auto json(req->getJsonObject());
        if (!json) {
            co_return errorResponse(Error::ErrBadRequest, req->getJsonError(), callback);
        }
        if (const auto error = validateJson(schemas::repositoryMigration, *json)) {
            co_return simpleError(Error::ErrBadRequest, error->msg, callback);
        }

        const auto repo = (*json)["repo"].asString();

        const auto newRepo = co_await github_.getNewRepositoryLocation(repo);
        if (newRepo.empty()) {
            co_return simpleError(Error::ErrBadRequest, "not_moved", callback);
        }

        if (const auto canAccess = co_await validateRepositoryAccess(newRepo, token, callback); !canAccess) {
            co_return;
        }

        if (const auto error = co_await database_.updateRepository(repo, newRepo); error != Error::Ok) {
            logger.error("Failed to update repositories for {} in database", repo);
            co_return simpleError(Error::ErrInternal, "internal", callback);
        }

        co_await github_.invalidateCache(repo);

        Json::Value root;
        root["message"] = "Repository migrated successfully";
        const auto resp = HttpResponse::newHttpJsonResponse(root);
        resp->setStatusCode(k200OK);
        callback(resp);
    }

    Task<> ProjectsController::invalidate(HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                          const std::string id, const std::string token) const {
        const auto project = co_await validateProjectAccess(id, token, callback);
        if (!project) {
            co_return;
        }

        co_await github_.invalidateCache(project->getValueOfSourceRepo());
        co_await documentation_.invalidateCache(*project);

        Json::Value root;
        root["message"] = "Project cache invalidated successfully";
        const auto resp = HttpResponse::newHttpJsonResponse(root);
        resp->setStatusCode(k200OK);
        callback(resp);
    }
}
