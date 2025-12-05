#include "project_management.h"

#include <include/uri.h>

#include <service/database/database.h>
#include <service/error.h>
#include <service/external/frontend.h>
#include <service/project/cached/cached.h>
#include <service/util.h>
#include <storage/storage.h>
#include <util/crypto.h>

using namespace drogon;
using namespace logging;

namespace service {
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

    bool isOwner(const nlohmann::json &owners, const std::string &username) {
        const auto lowerUsername = strToLower(username);
        for (const auto &owner: owners) {
            if (strToLower(owner.get<std::string>()) == lowerUsername) {
                return true;
            }
        }
        return false;
    }

    nlohmann::json processPlatforms(const nlohmann::json &metadata) {
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

    Task<PlatformProject> validatePlatform(const std::string id, const std::string repo, const std::string platform, const std::string slug,
                                           const bool checkExisting, const User user, const bool localEnv) {
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

        bool skipCheck = localEnv;
        if (!skipCheck && checkExisting) {
            if (const auto proj = co_await global::database->getProjectSource(id); proj) {
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
     * - not_owner: Unauthorized to register project
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
    Task<ValidatedProjectData> validateProjectData(const nlohmann::json json, const User user, const bool checkExisting,
                                                   const bool localEnv) {
        static const std::set<std::string> allowedProtocols = {"http", "https"};

        // Required
        const std::string branch = json["branch"];
        const std::string repo = json["repo"];
        const std::string path = json["path"];

        try {
            const uri repoUri(repo);

            if (!localEnv && !allowedProtocols.contains(repoUri.get_scheme())) {
                throw ApiException(Error::ErrBadRequest, "no_repository");
            }
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

        if (!localEnv && !checkExisting && resolved->contains("owners") && !isOwner((*resolved)["owners"], user.getValueOfId())) {
            throw ApiException(Error::ErrBadRequest, "not_owner");
        }

        const std::string id = (*resolved)["id"];
        if (id == ResourceLocation::DEFAULT_NAMESPACE || id.size() < 2) {
            throw ApiException(Error::ErrBadRequest, "illegal_id");
        }

        const std::string modid = resolved->contains("modid") ? (*resolved)["modid"] : "";
        if (!modid.empty()) {
            if (modid == ResourceLocation::DEFAULT_NAMESPACE || modid == ResourceLocation::COMMON_NAMESPACE) {
                throw ApiException(Error::ErrBadRequest, "illegal_modid");
            }
        }

        const auto platforms = processPlatforms(*resolved);
        if (platforms.empty()) {
            throw ApiException(Error::ErrBadRequest, "no_platforms");
        }

        std::unordered_map<std::string, PlatformProject> platformProjects;
        for (const auto &[key, val]: platforms.items()) {
            const auto platformProj = co_await validatePlatform(id, repo, key, val, checkExisting, user, localEnv);
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
        if (!modid.empty()) {
            project.setModid(modid);
        }

        co_return ValidatedProjectData{.project = project, .platforms = platforms};
    }

    void enqueueDeploy(const Project &project, const std::string &userId) {
        const auto currentLoop = trantor::EventLoop::getEventLoopOfCurrentThread();
        currentLoop->queueInLoop(async_func([project, userId]() -> Task<> {
            logger.debug("Deploying project '{}' from branch '{}'", project.getValueOfId(), project.getValueOfSourceBranch());

            if (const auto result = co_await global::storage->deployProject(project, userId); result) {
                logger.debug("Project '{}' deployed successfully", project.getValueOfId());

                co_await clearProjectCache(project.getValueOfId());
                co_await global::frontend->revalidateProject(project.getValueOfId());
            } else {
                logger.error("Encountered error while deploying project '{}'", project.getValueOfId());
            }
        }));
    }
}
