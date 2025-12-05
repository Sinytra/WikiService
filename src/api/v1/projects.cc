#include "projects.h"
#include "base.h"
#include "error.h"

#include <models/UserProject.h>
#include <schemas/schemas.h>
#include <service/auth.h>
#include <service/error.h>
#include <service/project/cached/cached.h>
#include <service/storage/deployment.h>
#include <service/storage/management/project_management.h>
// ReSharper disable once CppUnusedIncludeDirective
#include <service/serializers.h>
#include <service/storage/gitops.h>
#include <service/util.h>
#include <version.h>

using namespace drogon;
using namespace service;
using namespace logging;

using namespace drogon::orm;

namespace api::v1 {
    ProjectsController::ProjectsController(const bool localEnv) : localEnv_(localEnv) {}

    Task<> ProjectsController::greet(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback) const {
        nlohmann::json root;
        root["status"] = "Service operational";
        root["version"] = PROJECT_VERSION;
        root["hash"] = PROJECT_GIT_HASH;

        callback(jsonResponse(root));
        co_return;
    }

    Task<> ProjectsController::listIDs(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback) const {
        const auto ids = co_await global::database->getProjectIDs();

        callback(jsonResponse(ids));
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

    Task<> ProjectsController::getProjects(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback) const {
        const auto json(BaseProjectController::validatedBody(req, schemas::getBulkProjects));
        const std::vector<std::string> ids = json["ids"];

        Json::Value root(Json::arrayValue);
        for (const auto &id: ids) {
            if (const auto project = co_await global::database->getProjectSource(id); project) {
                root.append(projectToJson(*project));
            }
        }
        callback(HttpResponse::newHttpJsonResponse(root));
    }

    Task<> ProjectsController::create(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback) const {
        const auto session{co_await global::auth->getSession(req)};
        const auto json(BaseProjectController::validatedBody(req, schemas::projectRegister));

        const std::string repo = json["repo"];
        const std::string branch = json["branch"];
        const std::string path = json["path"];

        if (co_await global::database->existsForRepo(repo, branch, path)) {
            throw ApiException(Error::ErrBadRequest, "exists");
        }

        auto validated = co_await validateProjectData(json, session.user, false, localEnv_);
        auto [project, platforms] = validated;
        project.setIsCommunity(false);

        if (co_await global::database->existsForData(project.getValueOfId(), platforms)) {
            throw ApiException(Error::ErrBadRequest, "exists");
        }

        const auto result = co_await global::database->createProject(project);
        if (!result) {
            logger.error("Failed to create project {} in database", project.getValueOfId());
            throw ApiException(Error::ErrInternal, "internal");
        }

        if (const auto assignResult = co_await global::database->assignUserProject(session.username, result->getValueOfId(), "member");
            !assignResult)
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
        const auto json(BaseProjectController::validatedBody(req, schemas::projectRegister));

        auto [project, platforms] = co_await validateProjectData(json, session.user, true, localEnv_);

        if (const auto proj = co_await global::database->getProjectSource(project.getValueOfId()); !proj) {
            notFound();
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

        if (const auto result = co_await global::database->removeProject(project.getValueOfId()); !result) {
            logger.error("Failed to delete project {} in database", id);
            throw ApiException(Error::ErrInternal, "internal");
        }

        global::storage->invalidateProject(project);
        co_await global::database->refreshFlatTagItemView();
        co_await clearProjectCache(project.getValueOfId());

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
        assertFound(project);

        if (co_await global::storage->getProjectStatus(*project) == ProjectStatus::LOADING) {
            throw ApiException(Error::ErrBadRequest, "pending_deployment");
        }

        enqueueDeploy(*project, username);

        Json::Value root;
        root["message"] = "Project deploy started successfully";
        callback(HttpResponse::newHttpJsonResponse(root));
    }

    Task<> ProjectsController::getContentPages(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                               const std::string id) const {
        const auto version = req->getOptionalParameter<std::string>("version");
        const auto resolved = co_await BaseProjectController::getUserProject(req, id, version, std::nullopt);
        const auto items{co_await resolved->getItemContentPages(getTableQueryParams(req))};
        callback(jsonResponse(items));
    }

    Task<> ProjectsController::getContentTags(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                              const std::string id) const {
        const auto version = req->getOptionalParameter<std::string>("version");
        const auto resolved = co_await BaseProjectController::getUserProject(req, id, version, std::nullopt);
        const auto tags{co_await resolved->getTags(getTableQueryParams(req))};
        callback(jsonResponse(tags));
    }

    Task<> ProjectsController::getTagItems(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                           const std::string id) const {
        const auto prefix = std::format("/api/v1/dev/projects/{}/content/tags/", id);
        const auto tag = req->getPath().substr(prefix.size());
        assertNonEmptyParam(tag);

        const auto version = req->getOptionalParameter<std::string>("version");
        const auto resolved = co_await BaseProjectController::getUserProject(req, id, version, std::nullopt);
        const auto items{co_await resolved->getTagItems(tag, getTableQueryParams(req))};
        callback(jsonResponse(items));
    }

    Task<> ProjectsController::getRecipes(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                          const std::string id) const {
        const auto resolved = co_await BaseProjectController::getProjectWithParams(req, id);
        const auto recipes{co_await resolved->getRecipes(getTableQueryParams(req))};
        callback(jsonResponse(recipes));
    }

    Task<> ProjectsController::getVersions(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                           const std::string id) const {
        const auto resolved = co_await BaseProjectController::getUserProject(req, id, std::nullopt, std::nullopt);
        const auto versions{co_await resolved->getVersions(getTableQueryParams(req))};
        callback(jsonResponse(versions));
    }

    Task<> ProjectsController::getDeployments(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                              const std::string id) const {
        const auto project = co_await BaseProjectController::getUserProject(req, id);
        const auto deployments = co_await global::database->getDeployments(project.getValueOfId(), getTableQueryParams(req).page);

        callback(jsonResponse(deployments));
    }

    Task<> ProjectsController::getDeployment(const HttpRequestPtr req, const std::function<void(const HttpResponsePtr &)> callback,
                                             const std::string id) const {
        const auto deployment(co_await global::database->findByPrimaryKey<Deployment>(id));
        assertFound(deployment);
        const auto project(co_await BaseProjectController::getUserProject(req, deployment->getValueOfProjectId()));

        Json::Value root(deployment->toJson());

        if (const auto revisionStr(deployment->getRevision()); revisionStr) {
            root["revision"] = parseJsonOrThrow(*revisionStr);

            const git::GitRevision revision = nlohmann::json::parse(*revisionStr);
            if (const auto url = git::formatCommitUrl(project, revision.fullHash); !url.empty()) {
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
        const auto deployment(co_await global::database->findByPrimaryKey<Deployment>(id));
        assertFound(deployment);

        // Prevent deletion of loading deployments
        if (deployment->getValueOfStatus() == enumToStr(DeploymentStatus::LOADING)) {
            throw ApiException(Error::ErrBadRequest, "deployment_loading");
        }

        const auto project(co_await BaseProjectController::getUserProject(req, deployment->getValueOfProjectId()));

        if (const auto result = co_await global::database->deleteDeployment(id); !result) {
            logger.error("Failed to delete deployment {} in database", id);
            throw ApiException(Error::ErrInternal, "internal");
        }

        if (deployment->getValueOfActive()) {
            logger.debug("Invalidating project after active deployment was removed");
            global::storage->removeDeployment(*deployment);
            co_await clearProjectCache(project.getValueOfId());
        }

        callback(jsonResponse(parkourJson(deployment->toJson())));
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
        const auto json(BaseProjectController::validatedBody(req, schemas::projectIssue));
        const auto resolved(co_await BaseProjectController::getProjectWithParams(req, id));

        const auto parsedLevel = parseProjectIssueLevel(json["level"]);
        if (parsedLevel == ProjectIssueLevel::UNKNOWN) {
            throw ApiException(Error::ErrBadRequest, "invalid_level");
        }

        const auto parsedType = parseProjectIssueType(json["type"]);
        if (parsedType == ProjectIssueType::UNKNOWN) {
            throw ApiException(Error::ErrBadRequest, "invalid_type");
        }

        const auto parsedSubject = parseProjectError(json["subject"]);
        if (parsedSubject == ProjectError::UNKNOWN) {
            throw ApiException(Error::ErrBadRequest, "invalid_subject");
        }

        const auto details = json["details"];
        const auto path = json["path"];
        auto resolvedPath = path;
        if (!path.empty()) {
            const auto filePath = resolved->getPagePath(path);
            if (!filePath) {
                throw ApiException(Error::ErrBadRequest, "invalid_path");
            }
            resolvedPath = *filePath;
        }
        const auto res = co_await global::storage->addProjectIssue(resolved, parsedLevel, parsedType, parsedSubject, details, resolvedPath);
        if (res == Error::ErrNotFound) {
            throw ApiException(Error::ErrNotFound, "not_found");
        }

        callback(statusResponse(res == Error::Ok ? k201Created : k409Conflict));
    }
}
