#include "projects.h"

#include <log/log.h>
#include <models/Project.h>
#include <models/UserProject.h>
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
    ProjectsController::ProjectsController(Auth &a, GitHub &gh, Platforms &pf, Database &db, Storage &s, CloudFlare &cf) :
        auth_(a), github_(gh), platforms_(pf), database_(db), storage_(s), cloudflare_(cf) {}

    nlohmann::json ProjectsController::processPlatforms(const Json::Value &metadata) const {
        const std::vector<std::string> &valid = platforms_.getAvailablePlatforms();
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

    Task<std::optional<PlatformProject>> ProjectsController::validatePlatform(const std::string &id, const std::string &repo,
                                                                              const std::string &platform, const std::string &slug,
                                                                              const bool checkExisting, const User user,
                                                                              std::function<void(const HttpResponsePtr &)> callback) const {
        if (platform == PLATFORM_CURSEFORGE && !platforms_.curseforge_.isAvailable()) {
            simpleError(Error::ErrBadRequest, "cf_unavailable", callback);
            co_return std::nullopt;
        }

        auto &platInstance = platforms_.getPlatform(platform);

        const auto platformProj = co_await platInstance.getProject(slug);
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
            if (const auto verified = co_await platInstance.verifyProjectAccess(*platformProj, user, repo); !verified) {
                simpleError(Error::ErrBadRequest, "ownership", callback, [&](Json::Value &root) {
                    root["details"] = "Platform: " + platform;
                    root["can_verify_mr"] = true; // TODO
                });
                co_return std::nullopt;
            }
        }

        co_return platformProj;
    }

    /**
     * Possible errors:
     * - unsupported: Community project registration is not yet implemented
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
    Task<std::optional<ValidatedProjectData>> ProjectsController::validateProjectData(const Json::Value &json, const std::string &token,
                                                                                      std::function<void(const HttpResponsePtr &)> callback,
                                                                                      const bool checkExisting) const {
        const auto session(co_await auth_.getSession(token));
        if (!session) {
            simpleError(Error::ErrUnauthorized, "unauthorized", callback);
            co_return std::nullopt;
        }

        // Required
        const auto branch = json["branch"].asString();
        const auto repo = json["repo"].asString();
        const auto path = json["path"].asString();
        // Optional
        const auto mrCode = json["mr_code"].asString();

        // TODO Rework this

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

        const auto platforms = processPlatforms(jsonContent);
        if (platforms.empty()) {
            simpleError(Error::ErrBadRequest, "no_platforms", callback);
            co_return std::nullopt;
        }

        std::unordered_map<std::string, PlatformProject> platformProjects;
        for (const auto &[key, val]: platforms.items()) {
            const auto platformProj = co_await validatePlatform(id, repo, key, val, checkExisting, session->user, callback);
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

        co_return ValidatedProjectData{.project = project, .platforms = platforms};
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
        const auto session(co_await auth_.getSession(token));
        if (!session) {
            simpleError(Error::ErrUnauthorized, "unauthorized", callback);
            co_return;
        }

        const auto projects = co_await database_.getUserProjects(session->username);
        Json::Value projectsJson(Json::arrayValue);
        for (const auto &project: projects) {
            Json::Value json = projectToJson(project);
            json["status"] = projectStatusToString(co_await storage_.getProjectStatus(project));
            projectsJson.append(json);
        }

        Json::Value root;
        root["profile"] = session->profile;
        root["projects"] = projectsJson;

        const auto resp = HttpResponse::newHttpJsonResponse(root);
        resp->setStatusCode(k200OK);
        callback(resp);
        co_return;
    }

    Task<> ProjectsController::getProject(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback, std::string id,
                                          std::string token) const {
        const auto session(co_await auth_.getSession(token));
        if (!session) {
            simpleError(Error::ErrUnauthorized, "unauthorized", callback);
            co_return;
        }

        const auto project = co_await database_.getUserProject(session->username, id);
        if (!project) {
            simpleError(Error::ErrBadRequest, "not_found", callback);
            co_return;
        }

        Json::Value root;
        if (const auto resolved = co_await storage_.maybeGetProject(*project)) {
            root = resolved->toJson();
        } else {
            root = projectToJson(*project);
        }
        root["status"] = projectStatusToString(co_await storage_.getProjectStatus(*project));

        const auto resp = HttpResponse::newHttpJsonResponse(root);
        resp->setStatusCode(k200OK);
        callback(resp);
        co_return;
    }

    Task<> ProjectsController::getProjectLog(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback, std::string id,
                                             std::string token) const {
        const auto session(co_await auth_.getSession(token));
        if (!session) {
            simpleError(Error::ErrUnauthorized, "unauthorized", callback);
            co_return;
        }

        const auto project = co_await database_.getUserProject(session->username, id);
        if (!project) {
            simpleError(Error::ErrBadRequest, "not_found", callback);
            co_return;
        }

        const auto log = storage_.getProjectLog(*project);
        if (!log) {
            simpleError(Error::ErrNotFound, "not_found", callback);
            co_return;
        }

        Json::Value root;
        root["content"] = *log;

        const auto resp = HttpResponse::newHttpJsonResponse(root);
        resp->setStatusCode(k200OK);
        callback(resp);
        co_return;
    }

    Task<> ProjectsController::listPopularProjects(HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback) const {
        const std::vector<std::string> ids = co_await cloudflare_.getMostVisitedProjectIDs();
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
        auto [project, platforms] = *validated;
        project.setIsCommunity(isCommunity);

        if (co_await database_.existsForData(project.getValueOfId(), platforms)) {
            co_return simpleError(Error::ErrBadRequest, "exists", callback);
        }

        if (const auto [result, resultErr] = co_await database_.createProject(project); resultErr != Error::Ok) {
            logger.error("Failed to create project {} in database", project.getValueOfId());
            co_return simpleError(Error::ErrInternal, "internal", callback);
        }

        reloadProject(project);

        Json::Value root;
        root["project"] = projectToJson(project);
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
        auto [project, platforms] = *validated;

        if (const auto [proj, projErr] = co_await database_.getProjectSource(project.getValueOfId()); !proj) {
            co_return simpleError(Error::ErrBadRequest, "not_found", callback);
        }

        if (const auto error = co_await database_.updateProject(project); error != Error::Ok) {
            logger.error("Failed to update project {} in database", project.getValueOfId());
            co_return simpleError(Error::ErrInternal, "internal", callback);
        }

        reloadProject(project);

        Json::Value root;
        root["message"] = "Project updated successfully";
        root["project"] = projectToJson(project);
        const auto resp = HttpResponse::newHttpJsonResponse(root);
        resp->setStatusCode(k200OK);
        callback(resp);
    }

    Task<> ProjectsController::remove(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback, const std::string id,
                                      const std::string token) const {
        const auto session(co_await auth_.getSession(token));
        if (!session) {
            simpleError(Error::ErrUnauthorized, "unauthorized", callback);
            co_return;
        }

        const auto project = co_await database_.getUserProject(session->username, id);
        if (!project) {
            simpleError(Error::ErrBadRequest, "not_found", callback);
            co_return;
        }

        // Repo perms verified, proceed to deletion

        if (const auto error = co_await database_.removeProject(id); error != Error::Ok) {
            logger.error("Failed to delete project {} in database", id);
            co_return simpleError(Error::ErrInternal, "internal", callback);
        }

        co_await storage_.invalidateProject(*project);

        Json::Value root;
        root["message"] = "Project deleted successfully";
        const auto resp = HttpResponse::newHttpJsonResponse(root);
        resp->setStatusCode(k200OK);
        callback(resp);

        co_return;
    }

    Task<> ProjectsController::invalidate(HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                          const std::string id, const std::string token) const {
        const auto session(co_await auth_.getSession(token));
        if (!session) {
            simpleError(Error::ErrUnauthorized, "unauthorized", callback);
            co_return;
        }

        const auto project = co_await database_.getUserProject(session->username, id);
        if (!project) {
            simpleError(Error::ErrBadRequest, "not_found", callback);
            co_return;
        }

        // Invalidate ahead of reloading
        co_await storage_.invalidateProject(*project);

        reloadProject(*project, false);

        Json::Value root;
        root["message"] = "Project cache invalidated successfully";
        const auto resp = HttpResponse::newHttpJsonResponse(root);
        resp->setStatusCode(k200OK);
        callback(resp);
    }

    void ProjectsController::reloadProject(const Project &project, bool invalidate) const {
        const auto currentLoop = trantor::EventLoop::getEventLoopOfCurrentThread();
        currentLoop->queueInLoop(async_func([this, project, invalidate]() -> Task<> {
            if (invalidate) {
                logger.debug("Invalidating project '{}'", project.getValueOfId());
                co_await storage_.invalidateProject(project);
            }

            logger.debug("Reloading project '{}'", project.getValueOfId());

            if (const auto [resolved, resErr](co_await storage_.getProject(project, std::nullopt, std::nullopt)); resErr == Error::Ok) {
                logger.debug("Project '{}' reloaded successfully", project.getValueOfId());
            } else {
                logger.error("Encountered error while reloading project '{}'", project.getValueOfId());
            }
        }));
    }
}
