#include "projects.h"

#include <database/database.h>
#include <include/uri.h>
#include <log/log.h>
#include <models/UserProject.h>
#include <service/auth.h>
#include <service/cloudflare.h>
#include <service/crypto.h>
#include <service/error.h>
#include <service/schemas.h>
// ReSharper disable once CppUnusedIncludeDirective
#include <service/serializers.h>
#include <service/storage/gitops.h>
#include <service/storage/storage.h>
#include <service/util.h>

#include "base.h"
#include "deployment.h"
#include "error.h"

#define MODID_MINECRAFT "minecraft"

using namespace std;
using namespace drogon;
using namespace service;
using namespace logging;

using namespace drogon::orm;
using namespace drogon_model::postgres;

namespace api::v1 {
    Task<bool> isProjectPubliclyBrowseable(const std::string &repo) {
        try {
            const uri repoUri(repo);
            const auto client = createHttpClient(repo);
            const auto httpReq = HttpRequest::newHttpRequest();
            httpReq->setMethod(Get);
            httpReq->setPath("/" + repoUri.get_path());
            const auto response = co_await client->sendRequestCoro(httpReq);
            const auto status = response->getStatusCode();
            co_return status == k200OK;
        } catch (const std::exception &e) {
            logger.error("Error checking public status: {}", e.what());
            co_return false;
        }
    }

    nlohmann::json ProjectsController::processPlatforms(const nlohmann::json &metadata) const {
        const std::vector<std::string> &valid = global::platforms->getAvailablePlatforms();
        nlohmann::json projects(nlohmann::json::value_t::object);
        if (metadata.contains("platforms") && metadata["platforms"].is_object()) {
            const auto platforms = metadata["platforms"];
            for (const auto &platform: valid) {
                if (platforms.contains(platform) && platforms[platform].is_string()) {
                    projects[platform] = platforms[platform];
                }
            }
        }
        if (metadata.contains("platform") && metadata["platform"].is_string() && !projects.contains(metadata["platform"]) &&
            metadata.contains("slug") && metadata["slug"].is_string())
        {
            projects[metadata["platform"]] = metadata["slug"];
        }
        return projects;
    }

    Task<PlatformProject> ProjectsController::validatePlatform(const std::string &id, const std::string &repo, const std::string &platform,
                                                               const std::string &slug, const bool checkExisting, const User user,
                                                               std::function<void(const HttpResponsePtr &)> callback) const {
        if (platform == PLATFORM_CURSEFORGE && !global::platforms->curseforge_.isAvailable()) {
            throw ApiException(Error::ErrBadRequest, "cf_unavailable");
        }

        auto &platInstance = global::platforms->getPlatform(platform);

        const auto platformProj = co_await platInstance.getProject(slug);
        if (!platformProj) {
            throw ApiException(Error::ErrBadRequest, "no_project", [&](Json::Value &root) { root["details"] = "Platform: " + platform; });
        }

        if (platformProj->type == _unknown) {
            throw ApiException(Error::ErrBadRequest, "unsupported_type",
                               [&](Json::Value &root) { root["details"] = "Platform: " + platform; });
        }

        bool skipCheck = false;
        if (checkExisting) {
            if (const auto [proj, projErr] = co_await global::database->getProjectSource(id); proj) {
                if (const auto platforms = parseJsonString(proj->getValueOfPlatforms());
                    platforms && platforms->isMember(platform) && (*platforms)[platform] == slug)
                {
                    skipCheck = true;
                }
            }
        }
        if (!skipCheck) {
            if (const auto verified = co_await platInstance.verifyProjectAccess(*platformProj, user, repo); !verified) {
                throw ApiException(Error::ErrBadRequest, "ownership", [&](Json::Value &root) {
                    root["details"] = "Platform: " + platform;
                    root["can_verify_mr"] = platform == PLATFORM_MODRINTH && !user.getModrinthId();
                });
            }
        }

        co_return *platformProj;
    }

    /**
     * Resolution errors:
     * - internal: Internal server error
     * - unknown: Internal server error
     * Platform errors:
     * - no_platforms: No defined platform projects
     * - no_project: Platform project not found
     * - unsupported_type: Unsupported platform project type
     * - ownership: Failed to verify platform project ownership (data: can_verify_mr)
     * - cf_unavailable: CF Project registration unavailable due to missing API key
     * Project errors:
     * - requires_auth: Repository connection requires auth
     * - no_repository: Repository not found
     * - no_branch: Wrong branch specified
     * - no_path: Invalid path / metadata not found at given path
     * - invalid_meta: Metadata file format is invalid
     */
    Task<ValidatedProjectData> ProjectsController::validateProjectData(const Json::Value &json, const User user,
                                                                       std::function<void(const HttpResponsePtr &)> callback,
                                                                       const bool checkExisting) const {
        // Required
        const auto branch = json["branch"].asString();
        const auto repo = json["repo"].asString();
        const auto path = json["path"].asString();

        try {
            const uri repoUri(repo);
        } catch (const std::exception &e) {
            logger.error("Invalid repository URL provided: {} Error: {}", repo, e.what());
            throw ApiException(Error::ErrBadRequest, "no_repository");
        }

        Project tempProject;
        tempProject.setId("_temp-" + crypto::generateSecureRandomString(8));
        tempProject.setSourceRepo(repo);
        tempProject.setSourceBranch(branch);
        tempProject.setSourcePath(path);

        const auto [resolved, err, details] = co_await global::storage->setupValidateTempProject(tempProject);
        if (err != ProjectError::OK) {
            throw ApiException(Error::ErrBadRequest, enumToStr(err), [&details](Json::Value &root) { root["details"] = details; });
        }

        const std::string id = (*resolved)["id"];
        if (id == MODID_MINECRAFT || id.size() < 2) {
            throw ApiException(Error::ErrBadRequest, "illegal_id");
        }

        const auto platforms = processPlatforms(*resolved);
        if (platforms.empty()) {
            throw ApiException(Error::ErrBadRequest, "no_platforms");
        }

        std::unordered_map<std::string, PlatformProject> platformProjects;
        for (const auto &[key, val]: platforms.items()) {
            const auto platformProj = co_await validatePlatform(id, repo, key, val, checkExisting, user, callback);
            platformProjects.emplace(key, platformProj);
        }
        const auto &preferredProj =
            platforms.contains(PLATFORM_MODRINTH) ? platformProjects[PLATFORM_MODRINTH] : platformProjects.begin()->second;

        const auto isPublic = co_await isProjectPubliclyBrowseable(repo);

        Project project;
        project.setId(id);
        project.setName(preferredProj.name);
        project.setSourceRepo(repo);
        project.setSourceBranch(branch);
        project.setSourcePath(path);
        project.setType(projectTypeToString(preferredProj.type));
        project.setPlatforms(platforms.dump());
        project.setIsPublic(isPublic);

        co_return ValidatedProjectData{.project = project, .platforms = platforms};
    }

    Task<> ProjectsController::greet(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback) const {
        const auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k200OK);
        resp->setBody("Service operational");
        callback(resp);
        co_return;
    }

    Task<> ProjectsController::listIDs(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback) const {
        const auto ids = co_await global::database->getProjectIDs();

        Json::Value root(Json::arrayValue);
        for (const auto &id: ids) {
            root.append(id);
        }

        callback(HttpResponse::newHttpJsonResponse(root));
    }

    Task<> ProjectsController::listUserProjects(const HttpRequestPtr req,
                                                const std::function<void(const HttpResponsePtr &)> callback) const {
        const auto session(co_await global::auth->getSession(req));
        const auto projects = co_await global::database->getUserProjects(session.username);

        Json::Value projectsJson(Json::arrayValue);
        for (const auto &project: projects) {
            Json::Value json = projectToJson(project, true);
            json["status"] = enumToStr(co_await global::storage->getProjectStatus(project));
            projectsJson.append(json);
        }

        Json::Value root;
        root["profile"] = session.profile;
        root["projects"] = projectsJson;

        callback(HttpResponse::newHttpJsonResponse(root));
        co_return;
    }

    Task<> ProjectsController::getProject(const HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback,
                                          const std::string id) const {
        const auto project{co_await BaseProjectController::getUserProject(req, id)};

        Json::Value root;
        if (const auto resolved = co_await global::storage->maybeGetProject(project)) {
            root = co_await resolved->toJson(true);
        } else {
            root = projectToJson(project, true);
        }
        root["status"] = enumToStr(co_await global::storage->getProjectStatus(project));
        root["has_active_deployment"] = (co_await global::database->getActiveDeployment(id)).has_value();

        callback(HttpResponse::newHttpJsonResponse(root));
    }

    Task<> ProjectsController::getProjectLog(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                             const std::string id) const {
        const auto project{co_await BaseProjectController::getUserProject(req, id)};
        const auto log{global::storage->getProjectLog(project)};
        if (!log) {
            throw ApiException(Error::ErrNotFound, "not_found");
        }

        Json::Value root;
        root["content"] = *log;

        callback(HttpResponse::newHttpJsonResponse(root));
    }

    Task<> ProjectsController::listPopularProjects(const HttpRequestPtr req,
                                                   const std::function<void(const HttpResponsePtr &)> callback) const {
        const std::vector<std::string> ids = co_await global::cloudFlare->getMostVisitedProjectIDs();
        Json::Value root(Json::arrayValue);
        for (const auto &id: ids) {
            if (const auto [project, error] = co_await global::database->getProjectSource(id); project) {
                root.append(projectToJson(*project));
            }
        }
        callback(HttpResponse::newHttpJsonResponse(root));
    }

    Task<> ProjectsController::create(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback) const {
        const auto session{co_await global::auth->getSession(req)};
        auto json(req->getJsonObject());
        if (!json) {
            throw ApiException(Error::ErrBadRequest, req->getJsonError());
        }
        if (const auto error = validateJson(schemas::projectRegister, *json)) {
            throw ApiException(Error::ErrBadRequest, error->msg);
        }

        const auto isCommunity = (*json)["is_community"].asBool();

        // TODO Add support for community projects
        if (isCommunity) {
            throw ApiException(Error::ErrInternal, "unsupported");
        }

        const auto repo = (*json)["repo"].asString();
        const auto branch = (*json)["branch"].asString();
        const auto path = (*json)["path"].asString();

        if (co_await global::database->existsForRepo(repo, branch, path)) {
            throw ApiException(Error::ErrBadRequest, "exists");
        }

        auto validated = co_await validateProjectData(*json, session.user, callback, false);
        auto [project, platforms] = validated;
        project.setIsCommunity(isCommunity);

        if (co_await global::database->existsForData(project.getValueOfId(), platforms)) {
            throw ApiException(Error::ErrBadRequest, "exists");
        }

        const auto [result, resultErr] = co_await global::database->createProject(project);
        if (resultErr != Error::Ok) {
            logger.error("Failed to create project {} in database", project.getValueOfId());
            throw ApiException(Error::ErrInternal, "internal");
        }

        if (const auto assignErr = co_await global::database->assignUserProject(session.username, result->getValueOfId(), "member");
            assignErr != Error::Ok)
        {
            logger.error("Failed to assign project {} to user {}", result->getValueOfId(), session.username);
            co_await global::database->removeProject(result->getValueOfId());
            throw ApiException(Error::ErrInternal, "internal");
        }

        Json::Value root;
        root["project"] = projectToJson(*result, true);
        root["message"] = "Project registered successfully";
        callback(HttpResponse::newHttpJsonResponse(root));

        enqueueDeploy(*result, session.username);
    }

    Task<> ProjectsController::update(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback) const {
        const auto session{co_await global::auth->getSession(req)};

        const auto json(req->getJsonObject());
        if (!json) {
            throw ApiException(Error::ErrBadRequest, req->getJsonError());
        }
        if (const auto error = validateJson(schemas::projectRegister, *json)) {
            throw ApiException(Error::ErrBadRequest, error->msg);
        }

        auto [project, platforms] = co_await validateProjectData(*json, session.user, callback, true);

        if (const auto [proj, projErr] = co_await global::database->getProjectSource(project.getValueOfId()); !proj) {
            throw ApiException(Error::ErrNotFound, "not_found");
        }

        if (const auto res = co_await global::database->updateModel(project); !res) {
            logger.error("Failed to update project {} in database", project.getValueOfId());
            throw ApiException(Error::ErrInternal, "internal");
        }

        Json::Value root;
        root["message"] = "Project updated successfully";
        root["project"] = projectToJson(project, true);
        callback(HttpResponse::newHttpJsonResponse(root));

        enqueueDeploy(project, session.username);
    }

    Task<> ProjectsController::remove(const HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback,
                                      const std::string id) const {
        const auto project = co_await BaseProjectController::getUserProject(req, id);

        if (const auto error = co_await global::database->removeProject(project.getValueOfId()); error != Error::Ok) {
            logger.error("Failed to delete project {} in database", id);
            throw ApiException(Error::ErrInternal, "internal");
        }

        co_await global::storage->invalidateProject(project);

        callback(simpleResponse("Project deleted successfully"));
    }

    Task<> ProjectsController::deployProject(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                             const std::string id) const {
        std::string username = "";

        const auto token = req->getParameter("token");
        if (const auto user = co_await global::auth->getGitHubTokenUser(token)) {
            username = user->getValueOfId();
        }

        if (username.empty()) {
            const auto session{co_await global::auth->getSession(req)};
            username = session.username;
        }

        const auto project = co_await global::database->getUserProject(username, id);
        if (!project) {
            throw ApiException(Error::ErrNotFound, "not_found");
        }

        if (co_await global::storage->getProjectStatus(*project) == ProjectStatus::LOADING) {
            throw ApiException(Error::ErrBadRequest, "pending_deployment");
        }

        enqueueDeploy(*project, username);

        Json::Value root;
        root["message"] = "Project deploy started successfully";
        callback(HttpResponse::newHttpJsonResponse(root));
    }

    void ProjectsController::enqueueDeploy(const Project &project, const std::string userId) const {
        const auto currentLoop = trantor::EventLoop::getEventLoopOfCurrentThread();
        currentLoop->queueInLoop(async_func([this, project, userId]() -> Task<> {
            logger.debug("Deploying project '{}' from branch '{}'", project.getValueOfId(), project.getValueOfSourceBranch());

            if (const auto [resolved, resErr](co_await global::storage->deployProject(project, userId)); resErr == Error::Ok) {
                logger.debug("Project '{}' deployed successfully", project.getValueOfId());
            } else {
                logger.error("Encountered error while deploying project '{}'", project.getValueOfId());
            }
        }));
    }

    Task<> ProjectsController::getContentPages(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                               const std::string id) const {
        const auto version = req->getOptionalParameter<std::string>("version");
        const auto resolved = co_await BaseProjectController::getUserProject(req, id, version, std::nullopt);
        const auto items{co_await resolved.getItems(getTableQueryParams(req))};
        callback(jsonResponse(items));
    }

    Task<> ProjectsController::getContentTags(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                              const std::string id) const {
        const auto version = req->getOptionalParameter<std::string>("version");
        const auto resolved = co_await BaseProjectController::getUserProject(req, id, version, std::nullopt);
        const auto tags{co_await resolved.getTags(getTableQueryParams(req))};
        callback(jsonResponse(tags));
    }

    Task<> ProjectsController::getTagItems(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                           const std::string id) const {
        const auto prefix = std::format("/api/v1/dev/projects/{}/content/tags/", id);
        const auto tag = req->getPath().substr(prefix.size());
        if (tag.empty()) {
            throw ApiException(Error::ErrBadRequest, "Missing tag parameter");
        }

        const auto version = req->getOptionalParameter<std::string>("version");
        const auto resolved = co_await BaseProjectController::getUserProject(req, id, version, std::nullopt);
        const auto items{co_await resolved.getTagItems(tag, getTableQueryParams(req))};
        callback(jsonResponse(items));
    }

    Task<> ProjectsController::getRecipes(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                          const std::string id) const {
        const auto version = req->getOptionalParameter<std::string>("version");
        const auto resolved = co_await BaseProjectController::getUserProject(req, id, version, std::nullopt);
        const auto recipes{co_await resolved.getRecipes(getTableQueryParams(req))};
        callback(jsonResponse(recipes));
    }

    Task<> ProjectsController::getVersions(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                           const std::string id) const {
        const auto resolved = co_await BaseProjectController::getUserProject(req, id, std::nullopt, std::nullopt);
        const auto versions{co_await resolved.getVersions(getTableQueryParams(req))};
        callback(jsonResponse(versions));
    }

    Task<> ProjectsController::getDeployments(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                              const std::string id) const {
        const auto project = co_await BaseProjectController::getUserProject(req, id);
        auto deployments = co_await global::database->getDeployments(project.getValueOfId(), getTableQueryParams(req).page);

        callback(jsonResponse(deployments));
    }

    Task<> ProjectsController::getDeployment(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                             const std::string id) const {
        const auto deployment(co_await global::database->getModel<Deployment>(id));
        if (!deployment) {
            throw ApiException(Error::ErrNotFound, "not_found");
        }
        const auto project(co_await BaseProjectController::getUserProject(req, deployment->getValueOfProjectId()));

        Json::Value root(deployment->toJson());

        if (const auto revisionStr(deployment->getRevision()); revisionStr) {
            root["revision"] = parseJsonOrThrow(*revisionStr);

            const git::GitRevision revision = nlohmann::json::parse(*revisionStr);
            if (const auto url = formatCommitUrl(project, revision.fullHash); !url.empty()) {
                root["revision"]["url"] = url;
            }
        } else {
            root["revision"] = Json::nullValue;
        }

        const auto issues(co_await global::database->getDeploymentIssues(id));
        root["issues"] = toJson(issues);

        callback(HttpResponse::newHttpJsonResponse(root));
    }

    Task<> ProjectsController::deleteDeployment(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                                const std::string id) const {
        const auto deployment(co_await global::database->getModel<Deployment>(id));
        if (!deployment) {
            throw ApiException(Error::ErrNotFound, "not_found");
        }

        // Prevent deletion of loading deployments
        if (deployment->getValueOfStatus() == enumToStr(DeploymentStatus::LOADING)) {
            throw ApiException(Error::ErrBadRequest, "deployment_loading");
        }

        const auto project(co_await BaseProjectController::getUserProject(req, deployment->getValueOfProjectId()));

        if (const auto error = co_await global::database->deleteDeployment(id); error != Error::Ok) {
            logger.error("Failed to delete deployment {} in database", id);
            throw ApiException(Error::ErrInternal, "internal");
        }

        if (deployment->getValueOfActive()) {
            logger.debug("Invalidating project after active deployment was removed");
            co_await global::storage->invalidateProject(project);
        }

        callback(simpleResponse("Deployment deleted successfully"));
    }

    Task<> ProjectsController::getIssues(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                         const std::string id) const {
        const auto project(co_await BaseProjectController::getUserProject(req, id));

        const auto deployment(co_await global::database->getActiveDeployment(id));
        const auto issues =
            deployment ? co_await global::database->getDeploymentIssues(deployment->getValueOfId()) : std::vector<ProjectIssue>();

        callback(HttpResponse::newHttpJsonResponse(toJson(issues)));
    }

    Task<> ProjectsController::addIssue(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                        const std::string id) const {
        const auto json(req->getJsonObject());
        if (!json) {
            throw ApiException(Error::ErrBadRequest, req->getJsonError());
        }
        if (const auto error = validateJson(schemas::projectIssue, *json)) {
            throw ApiException(Error::ErrBadRequest, error->msg);
        }

        const auto resolved(co_await BaseProjectController::getProject(id, req->getOptionalParameter<std::string>("version"),
                                                                       req->getOptionalParameter<std::string>("locale")));

        const auto parsedLevel = parseProjectIssueLevel((*json)["level"].asString());
        if (parsedLevel == ProjectIssueLevel::UNKNOWN) {
            throw ApiException(Error::ErrBadRequest, "invalid_level");
        }

        const auto parsedType = parseProjectIssueType((*json)["type"].asString());
        if (parsedType == ProjectIssueType::UNKNOWN) {
            throw ApiException(Error::ErrBadRequest, "invalid_type");
        }

        const auto parsedSubject = parseProjectError((*json)["subject"].asString());
        if (parsedSubject == ProjectError::UNKNOWN) {
            throw ApiException(Error::ErrBadRequest, "invalid_subject");
        }

        const auto details = (*json)["details"].asString();
        const auto path = (*json)["path"].asString();
        const auto res = co_await global::storage->addProjectIssue(resolved, parsedLevel, parsedType, parsedSubject, details, path);

        if (res == Error::ErrNotFound) {
            throw ApiException(Error::ErrNotFound, "not_found");
        }

        callback(statusResponse(res == Error::Ok ? k201Created : k409Conflict));
    }
}
