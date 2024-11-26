#include "platforms.h"

#define WIKI_USER_AGENT "Sinytra/modded-wiki/1.0.0"
#define MODRINTH_API_URL "https://api.modrinth.com/"
#define CURSEFORGE_API_URL "https://api.curseforge.com/"
#define MC_GAME_ID 432

#include <drogon/drogon.h>

#include "log/log.h"
#include "util.h"

const std::map<int, ProjectType> curseforgeProjectTypes = {{6, mod},       {12, resourcepack}, {6945, datapack},
                                                           {6552, shader}, {4471, modpack},    {5, plugin}};

static std::unordered_map<std::string, ProjectType> const projectTypeLookup = {
    {"mod", mod}, {"resourcepack", resourcepack}, {"datapack", datapack}, {"shader", shader}, {"modpack", modpack}, {"plugin", plugin}};

using namespace logging;
using namespace drogon;

namespace service {
    ProjectType getProjectType(const std::string &name) {
        if (const auto it = projectTypeLookup.find(name); it != projectTypeLookup.end()) {
            return it->second;
        }
        logger.error("Unknown project type '{}'", name);
        return _unknown;
    }

    std::string projectTypeToString(const ProjectType &type) {
        for (const auto &[key, val] : projectTypeLookup) {
            if (type == val) {
                return key;
            }
        }
        throw std::runtime_error("Cannot stringify unknown project type");
    }

    ModrinthPlatform::ModrinthPlatform(const std::string &clientId, const std::string &clientSecret, const std::string &redirectUrl) :
        clientId_(clientId), clientSecret_(clientSecret), redirectUrl_(redirectUrl) {}

    bool ModrinthPlatform::isOAuthConfigured() const { return !clientId_.empty() && !clientSecret_.empty() && !redirectUrl_.empty(); }

    Task<std::optional<std::string>> ModrinthPlatform::getOAuthToken(std::string code) const {
        const auto client = createHttpClient(MODRINTH_API_URL);
        const auto httpReq = HttpRequest::newHttpFormPostRequest();
        httpReq->setPath("/_internal/oauth/token");
        httpReq->addHeader("Authorization", clientSecret_);
        httpReq->setBody(std::format("grant_type=authorization_code&client_id={}&code={}&redirect_uri={}", clientId_, code, redirectUrl_));

        if (const auto response = co_await client->sendRequestCoro(httpReq); response && isSuccess(response->getStatusCode())) {
            if (const auto jsonResp = response->getJsonObject()) {
                const auto accessToken = (*jsonResp)["access_token"].asString();
                co_return accessToken;
            }
        }

        co_return std::nullopt;
    }

    Task<std::optional<std::string>> ModrinthPlatform::getAuthenticatedUser(std::string token) const {
        const auto client = createHttpClient(MODRINTH_API_URL);
        if (const auto [resp, err] = co_await sendApiRequest(
                client, Get, "/v3/user", [&token](const HttpRequestPtr &req) { req->addHeader("Authorization", token); });
            resp && resp->isObject())
        {
            const auto username = (*resp)["username"].asString();
            co_return username;
        }
        co_return std::nullopt;
    }

    Task<bool> ModrinthPlatform::isProjectMember(std::string slug, std::string username) const {
        const auto client = createHttpClient(MODRINTH_API_URL);
        // Check direct project members
        if (const auto [resp, err] = co_await sendApiRequest(client, Get, std::format("/v3/project/{}/members", slug));
            resp && resp->isArray())
        {
            for (const auto &item: *resp) {
                if (const auto memberUsername = item["user"]["username"].asString(); memberUsername == username) {
                    co_return true;
                }
            }
        }
        // Check organization members
        if (const auto [resp, err] = co_await sendApiRequest(client, Get, std::format("/v3/project/{}/organization", slug));
            resp && resp->isObject())
        {
            for (const auto members = (*resp)["members"]; const auto &item: members) {
                if (const auto memberUsername = item["user"]["username"].asString(); memberUsername == username) {
                    co_return true;
                }
            }
        }
        co_return false;
    }

    Task<std::optional<PlatformProject>> ModrinthPlatform::getProject(const std::string slug) {
        const auto client = createHttpClient(MODRINTH_API_URL);
        if (const auto [resp, err] = co_await sendApiRequest(client, Get, "/v3/project/" + slug); resp && resp->isObject()) {
            const auto projectSlug = (*resp)["slug"].asString();
            const auto name = (*resp)["name"].asString();
            const auto sourceUrl = resp->isMember("link_urls") && (*resp)["link_urls"].isMember("source")
                                       ? (*resp)["link_urls"]["source"]["url"].asString()
                                       : "";
            const auto projectTypes = (*resp)["project_types"];
            const auto type = projectTypes.empty() ? _unknown : getProjectType(projectTypes.front().asString());
            co_return PlatformProject{.slug = projectSlug, .name = name, .sourceUrl = sourceUrl, .type = type};
        }
        co_return std::nullopt;
    }

    CurseForgePlatform::CurseForgePlatform(std::string apiKey) : apiKey_(std::move(apiKey)) {}

    bool CurseForgePlatform::isAvailable() const { return !apiKey_.empty(); }

    Task<std::optional<PlatformProject>> CurseForgePlatform::getProject(std::string slug) {
        const auto client = createHttpClient(CURSEFORGE_API_URL);
        if (const auto [resp, err] =
                co_await sendAuthenticatedRequest(client, Get, std::format("/v1/mods/search?gameId={}&slug={}", MC_GAME_ID, slug), apiKey_);
            resp && resp->isObject())
        {
            const auto pagination = (*resp)["pagination"];
            const auto resultCount = pagination["resultCount"].asInt();
            if (const auto data = (*resp)["data"]; resultCount == 1 && data.size() == 1) {
                const auto &proj = data[0];
                const auto projectSlug = proj["slug"].asString();
                const auto name = proj["name"].asString();
                const auto sourceUrl =
                    proj.isMember("links") && proj["links"].isMember("sourceUrl") ? proj["links"]["sourceUrl"].asString() : "";
                const auto classId = proj["classId"].asInt();

                const auto type = curseforgeProjectTypes.find(classId);
                const auto actualType = type == curseforgeProjectTypes.end() ? _unknown : type->second;

                co_return PlatformProject{.slug = projectSlug, .name = name, .sourceUrl = sourceUrl, .type = actualType};
            }
        }
        co_return std::nullopt;
    }

    Platforms::Platforms(CurseForgePlatform &cf, ModrinthPlatform &mr) :
        curseforge_(cf), modrinth_(mr), platforms_({{PLATFORM_MODRINTH, modrinth_}, {PLATFORM_CURSEFORGE, curseforge_}}) {}

    Task<std::optional<PlatformProject>> Platforms::getProject(std::string platform, std::string slug) {
        const auto plat = platforms_.find(platform);

        if (plat == platforms_.end()) {
            co_return std::nullopt;
        }

        co_return co_await plat->second.getProject(slug);
    }
}
