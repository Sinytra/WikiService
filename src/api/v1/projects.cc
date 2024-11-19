#include "projects.h"

#include <models/Project.h>

#include "log/log.h"
#include "error.h"
#include "service/util.h"
#include "service/schemas.h"

using namespace std;
using namespace drogon;
using namespace service;
using namespace logging;

using namespace drogon::orm;
using namespace drogon_model::postgres;

namespace api::v1 {
    void simpleError(const service::Error &error, const std::string &message, std::function<void(const drogon::HttpResponsePtr &)> &callback) {
        Json::Value root;
        root["error"] = message;
        const auto resp = HttpResponse::newHttpJsonResponse(root);
        resp->setStatusCode(mapError(error));
        callback(resp);
    }

    Task<bool> verifyProjectOwnership(const PlatformProject& project, const std::string& repo, const std::string& mrCode, const std::string& redirectUrl) {
        const auto expected = "https://github.com/" + repo;
        if (!project.sourceUrl.empty() && expected.starts_with(repo)) {
            co_return true;
        }

        if (!mrCode.empty()) {

        }

        co_return false;
    }

    ProjectsController::ProjectsController(service::GitHub &gh, Platforms &pf, Database &db) : github_(gh), platforms_(pf), database_(db) {}

    Task<> ProjectsController::create(drogon::HttpRequestPtr req, std::function<void(const drogon::HttpResponsePtr &)> callback, std::string token) const {
        /*
         * https://github.com/apps/sinytra-modded-mc-wiki/installations/new
         *
         *  owner: z.string(),
            repo: z.string(),
            branch: z.string(),
            path: z.string(),
            is_community: z.boolean().optional(),

            mr_code: z.string().nullish()
         * */

        const auto installUrl = "https://github.com/apps/sinytra-modded-mc-wiki/installations/new";

        if (token.empty()) {
            co_return errorResponse(Error::ErrBadRequest, "Missing token parameter", callback);
        }

        auto json(req->getJsonObject());
        if (!json) {
            co_return errorResponse(Error::ErrBadRequest, req->getJsonError(), callback);
        }

        // Jump from one json library to the other. Parkour!
        const auto ser = serializeJsonString(*json);
        const auto nlJson = nlohmann::json::parse(ser);

        class custom_error_handler : public nlohmann::json_schema::basic_error_handler
        {
        private:
        std::function<void(const drogon::HttpResponsePtr &)> callback;
        public:
            custom_error_handler(std::function<void(const drogon::HttpResponsePtr &)> callback) : callback(callback) {}

            void error(const nlohmann::json_pointer<std::basic_string<char>> &pointer, const nlohmann::json &json1, const string &string1) {
                errorResponse(Error::ErrBadRequest, string1, callback);
            }
        };

        nlohmann::json_schema::json_validator validator;
        validator.set_root_schema(schemas::projectRegisterSchema);
        try {
            custom_error_handler err(callback);
            validator.validate(nlJson, err);
            if (err) {
                co_return;
            }
        } catch (const std::exception &e) {

        }

        // TODO Validate JSON

        const auto [username, usernameError](co_await github_.getUsername(token));
        if (!username) {
            Json::Value root;
            root["error"] = "user_github_auth";
            const auto resp = HttpResponse::newHttpJsonResponse(root);
            resp->setStatusCode(k400BadRequest);
            callback(resp);
            co_return;
        }

        // TODO Check community project / wiki admin perms

        const auto branch = (*json)["branch"].asString();
        const auto repo = (*json)["repo"].asString();
        const auto path = (*json)["path"].asString();

        const auto [installationId, installationIdError](co_await github_.getRepositoryInstallation(repo));
        if (!installationId) {
            Json::Value root;
            root["error"] = "missing_installation";
            root["install_url"] = installUrl;
            const auto resp = HttpResponse::newHttpJsonResponse(root);
            resp->setStatusCode(k400BadRequest);
            callback(resp);
            co_return;
        }

        const auto [installationToken, tokenErr](co_await github_.getInstallationToken(installationId.value()));
        if (!installationToken) {
            errorResponse(tokenErr, "GitHub authentication failure", callback);
            co_return;
        }

        const auto [perms, permsError](co_await github_.getCollaboratorPermissionLevel(repo, *username, *installationToken));
        if (!perms || perms != "admin" && perms != "write") {
            co_return simpleError(Error::ErrBadRequest, "insufficient_repo_perms", callback);
        }

        const auto [branches, branchesErr](co_await github_.getRepositoryBranches(repo, *installationToken));
        if (std::find(branches.begin(), branches.end(), branch) == branches.end()) {
            co_return simpleError(Error::ErrBadRequest, "no_branch", callback);
        }

        const auto metaFilePath = path + "/sinytra-wiki.json";
        const auto [contents, contentsError](co_await github_.getRepositoryJSONFile(repo, branch, metaFilePath, *installationToken));
        if (!contents) {
            co_return simpleError(Error::ErrBadRequest, "missing_meta", callback);
        }
        // TODO Validate
        const auto jsonContent = *contents;
        logger.info("Found meta: {}", jsonContent.toStyledString());

        const auto platformProj = co_await platforms_.getProject(jsonContent["platform"].asString(), jsonContent["slug"].asString());
        if (!platformProj) {
            co_return simpleError(Error::ErrBadRequest, "no_project", callback);
        }

//        const auto verified = co_await verifyProjectOwnership(platformProj->sourceUrl);
//        if (!verified) {
//
//        }

        Json::Value root;
        const auto resp = HttpResponse::newHttpJsonResponse(root);
        resp->setStatusCode(k200OK);
        callback(resp);

        co_return;
    }
}
