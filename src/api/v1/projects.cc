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
        if (const auto expected = "https://github.com/" + repo; !project.sourceUrl.empty() && expected.starts_with(repo)) {
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

    ProjectsController::ProjectsController(GitHub &gh, Platforms &pf, Database &db) : github_(gh), platforms_(pf), database_(db) {}

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
     * - cf_unavailable: CF Project registration unavailable due to missing API key
     * - no_project: Platform project not found
     * - ownership: Failed to verify platform project ownership (data: can_verify_mr)
     * - internal: Internal server error
     */
    Task<std::optional<Project>> ProjectsController::validateProjectData(const Json::Value &json, const std::string &token,
                                                                         std::function<void(const HttpResponsePtr &)> callback) const {
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
        const auto isCommunity = json["is_community"].asBool();

        // TODO Add support for community projects
        if (isCommunity) {
            simpleError(Error::ErrInternal, "unsupported", callback);
            co_return std::nullopt;
        }

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
        const auto platform = jsonContent["platform"].asString();
        const auto slug = jsonContent["slug"].asString();

        if (const auto [proj, projErr] = co_await database_.getProjectSource(id); proj) {
            simpleError(Error::ErrBadRequest, "exists", callback);
            co_return std::nullopt;
        }

        if (platform == PLATFORM_CURSEFORGE && !platforms_.curseforge_.isAvailable()) {
            simpleError(Error::ErrBadRequest, "cf_unavailable", callback);
            co_return std::nullopt;
        }

        const auto platformProj = co_await platforms_.getProject(platform, slug);
        if (!platformProj) {
            simpleError(Error::ErrBadRequest, "no_project", callback);
            co_return std::nullopt;
        }

        if (const auto verified = co_await verifyProjectOwnership(platforms_, *platformProj, repo, mrCode); !verified) {
            simpleError(Error::ErrBadRequest, "ownership", callback, [&](Json::Value &root) {
                root["can_verify_mr"] = platform == PLATFORM_MODRINTH && platforms_.modrinth_.isOAuthConfigured();
            });
            co_return std::nullopt;
        }

        Project project;
        project.setId(id);
        project.setName(platformProj->name);
        project.setSourceRepo(repo);
        project.setSourceBranch(branch);
        project.setSourcePath(path);
        project.setPlatform(platform);
        project.setSlug(slug);
        project.setIsCommunity(isCommunity);

        co_return project;
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

        const auto project = co_await validateProjectData(*json, token, callback);
        if (!project) {
            co_return;
        }

        if (const auto [result, resultErr] = co_await database_.createProject(*project); resultErr != Error::Ok) {
            logger.error("Failed to create project {} in database: {}", project->getValueOfId(), resultErr);
            co_return simpleError(Error::ErrInternal, "internal", callback);
        }

        Json::Value root;
        root["message"] = "Project registered successfully";
        const auto resp = HttpResponse::newHttpJsonResponse(root);
        resp->setStatusCode(k200OK);
        callback(resp);

        co_return;
    }
}
