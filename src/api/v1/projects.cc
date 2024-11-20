#include "projects.h"

#include <log/log.h>
#include <models/Project.h>
#include <service/schemas.h>
#include <service/util.h>

#include "error.h"

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

        const auto [username, usernameError](co_await github_.getUsername(token));
        if (!username) {
            co_return simpleError(Error::ErrBadRequest, "user_github_auth", callback);
        }

        // Required
        const auto branch = (*json)["branch"].asString();
        const auto repo = (*json)["repo"].asString();
        const auto path = (*json)["path"].asString();
        // Optional
        const auto mrCode = (*json)["mr_code"].asString();
        const auto isCommunity = (*json)["is_community"].asBool();

        // TODO Add support for community projects
        if (isCommunity) {
            co_return simpleError(Error::ErrInternal, "unsupported", callback);
        }

        const auto [installationId, installationIdError](co_await github_.getRepositoryInstallation(repo));
        if (!installationId) {
            co_return simpleError(Error::ErrBadRequest, "missing_installation", callback,
                                  [&](Json::Value &root) { root["install_url"] = github_.getAppInstallUrl(); });
        }

        const auto [installationToken, tokenErr](co_await github_.getInstallationToken(installationId.value()));
        if (!installationToken) {
            co_return simpleError(Error::ErrInternal, "github_auth", callback);
        }

        if (const auto [perms, permsError](co_await github_.getCollaboratorPermissionLevel(repo, *username, *installationToken));
            !perms || perms != "admin" && perms != "write")
        {
            co_return simpleError(Error::ErrBadRequest, "insufficient_repo_perms", callback);
        }

        if (const auto [branches, branchesErr](co_await github_.getRepositoryBranches(repo, *installationToken));
            ranges::find(branches, branch) == branches.end())
        {
            co_return simpleError(Error::ErrBadRequest, "no_branch", callback);
        }

        const auto metaFilePath = path + DOCS_META_FILE_PATH;
        const auto [contents, contentsError](co_await github_.getRepositoryJSONFile(repo, branch, metaFilePath, *installationToken));
        if (!contents) {
            co_return simpleError(Error::ErrBadRequest, "missing_meta", callback);
        }
        if (const auto error = validateJson(schemas::projectMetadata, *contents)) {
            co_return simpleError(Error::ErrBadRequest, error->msg, callback);
        }
        const auto jsonContent = *contents;
        const auto id = jsonContent["id"].asString();
        const auto platform = jsonContent["platform"].asString();
        const auto slug = jsonContent["slug"].asString();

        const auto platformProj = co_await platforms_.getProject(platform, slug);
        if (!platformProj) {
            co_return simpleError(Error::ErrBadRequest, "no_project", callback);
        }

        if (const auto verified = co_await verifyProjectOwnership(platforms_, *platformProj, repo, mrCode); !verified) {
            co_return simpleError(Error::ErrBadRequest, "ownership", callback, [&](Json::Value &root) {
                root["canVerifyModrinth"] = platform == PLATFORM_MODRINTH && platforms_.modrinth_.isOAuthConfigured();
            });
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

        if (const auto [result, resultErr] = co_await database_.createProject(project); resultErr != Error::Ok) {
            co_return simpleError(resultErr, "create_project", callback);
        }

        Json::Value root;
        root["message"] = "Project registered successfully";
        const auto resp = HttpResponse::newHttpJsonResponse(root);
        resp->setStatusCode(k200OK);
        callback(resp);

        co_return;
    }
}
