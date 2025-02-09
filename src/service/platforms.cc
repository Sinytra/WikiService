#include "platforms.h"

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

bool isNumber(const std::string &s) { return !s.empty() && std::ranges::all_of(s, isdigit); }

namespace service {
    ProjectType getProjectType(const std::string &name) {
        if (const auto it = projectTypeLookup.find(name); it != projectTypeLookup.end()) {
            return it->second;
        }
        logger.error("Unknown project type '{}'", name);
        return _unknown;
    }

    std::string projectTypeToString(const ProjectType &type) {
        for (const auto &[key, val]: projectTypeLookup) {
            if (type == val) {
                return key;
            }
        }
        throw std::runtime_error("Cannot stringify unknown project type");
    }

    Task<bool> DistributionPlatform::verifyProjectAccess(const PlatformProject project, const User user, const std::string repoUrl) {
        if (!project.sourceUrl.empty() && project.sourceUrl.starts_with(repoUrl)) {
            co_return true;
        }
        co_return false;
    }

    ModrinthPlatform::ModrinthPlatform() {}

    Task<std::optional<std::string>> ModrinthPlatform::getAuthenticatedUserID(std::string token) const {
        const auto client = createHttpClient(MODRINTH_API_URL);
        client->setUserAgent(WIKI_USER_AGENT);
        if (const auto [resp, err] = co_await sendApiRequest(
                client, Get, "/v3/user", [&token](const HttpRequestPtr &req) { req->addHeader("Authorization", token); });
            resp && resp->isObject())
        {
            const auto username = (*resp)["id"].asString();
            co_return username;
        }
        co_return std::nullopt;
    }

    Task<bool> ModrinthPlatform::isProjectMember(std::string slug, std::string userId) const {
        const auto client = createHttpClient(MODRINTH_API_URL);
        client->setUserAgent(WIKI_USER_AGENT);
        // Check direct project members
        if (const auto [resp, err] = co_await sendApiRequest(client, Get, std::format("/v3/project/{}/members", slug));
            resp && resp->isArray())
        {
            for (const auto &item: *resp) {
                if (const auto memberUsername = item["user"]["id"].asString(); memberUsername == userId) {
                    co_return true;
                }
            }
        }
        // Check organization members
        if (const auto [resp, err] = co_await sendApiRequest(client, Get, std::format("/v3/project/{}/organization", slug));
            resp && resp->isObject())
        {
            for (const auto members = (*resp)["members"]; const auto &item: members) {
                if (const auto memberUsername = item["user"]["id"].asString(); memberUsername == userId) {
                    co_return true;
                }
            }
        }
        co_return false;
    }

    Task<std::optional<PlatformProject>> ModrinthPlatform::getProject(const std::string slug) {
        const auto client = createHttpClient(MODRINTH_API_URL);
        client->setUserAgent(WIKI_USER_AGENT);
        if (const auto [resp, err] = co_await sendApiRequest(client, Get, "/v3/project/" + slug); resp && resp->isObject()) {
            const auto projectSlug = (*resp)["slug"].asString();
            const auto name = (*resp)["name"].asString();
            const auto sourceUrl = resp->isMember("link_urls") && (*resp)["link_urls"].isMember("source")
                                       ? (*resp)["link_urls"]["source"]["url"].asString()
                                       : "";
            const auto projectTypes = (*resp)["project_types"];
            const auto type = projectTypes.empty() ? _unknown : getProjectType(projectTypes.front().asString());
            co_return PlatformProject{
                .slug = projectSlug, .name = name, .sourceUrl = sourceUrl, .type = type, .platform = PLATFORM_MODRINTH};
        }
        co_return std::nullopt;
    }

    Task<bool> ModrinthPlatform::verifyProjectAccess(const PlatformProject project, const User user, const std::string repo) {
        if (co_await DistributionPlatform::verifyProjectAccess(project, user, repo)) {
            co_return true;
        }

        if (const auto id = user.getModrinthId()) {
            co_return co_await isProjectMember(project.slug, *id);
        }

        co_return false;
    }

    CurseForgePlatform::CurseForgePlatform(std::string apiKey) : apiKey_(std::move(apiKey)) {}

    bool CurseForgePlatform::isAvailable() const { return !apiKey_.empty(); }

    Task<std::optional<Json::Value>> CurseForgePlatform::getProjectData(std::string slug) const {
        const auto client = createHttpClient(CURSEFORGE_API_URL);

        // Likely a project ID
        if (isNumber(slug)) {
            if (const auto [resp, err] = co_await sendApiRequest(client, Get, std::format("/v1/mods/{}", slug),
                                                                 [&](const HttpRequestPtr &req) { req->addHeader("x-api-key", apiKey_); });
                resp && resp->isObject())
            {
                const auto data = (*resp)["data"];
                co_return data;
            }
        }

        if (const auto [resp, err] =
                co_await sendApiRequest(client, Get, std::format("/v1/mods/search?gameId={}&slug={}", MC_GAME_ID, slug),
                                        [&](const HttpRequestPtr &req) { req->addHeader("x-api-key", apiKey_); });
            resp && resp->isObject())
        {
            const auto pagination = (*resp)["pagination"];
            const auto resultCount = pagination["resultCount"].asInt();
            if (const auto data = (*resp)["data"]; resultCount == 1 && data.size() == 1) {
                co_return data[0];
            }
        }

        co_return std::nullopt;
    }

    Task<std::optional<PlatformProject>> CurseForgePlatform::getProject(std::string slug) {
        const auto data = co_await getProjectData(slug);
        if (!data) {
            co_return std::nullopt;
        }

        const auto proj = *data;
        const auto projectSlug = proj["slug"].asString();
        const auto name = proj["name"].asString();
        const auto sourceUrl = proj.isMember("links") && proj["links"].isMember("sourceUrl") ? proj["links"]["sourceUrl"].asString() : "";
        const auto classId = proj["classId"].asInt();

        const auto type = curseforgeProjectTypes.find(classId);
        const auto actualType = type == curseforgeProjectTypes.end() ? _unknown : type->second;

        co_return PlatformProject{
            .slug = projectSlug, .name = name, .sourceUrl = sourceUrl, .type = actualType, .platform = PLATFORM_CURSEFORGE};
    }

    Platforms::Platforms(CurseForgePlatform &cf, ModrinthPlatform &mr) :
        curseforge_(cf), modrinth_(mr), platforms_({{PLATFORM_MODRINTH, modrinth_}, {PLATFORM_CURSEFORGE, curseforge_}}) {}

    Task<std::optional<PlatformProject>> Platforms::getProject(const std::string platform, const std::string slug) {
        const auto plat = platforms_.find(platform);

        if (plat == platforms_.end()) {
            co_return std::nullopt;
        }

        co_return co_await plat->second.getProject(slug);
    }

    std::vector<std::string> Platforms::getAvailablePlatforms() {
        auto kv = std::views::keys(platforms_);
        return {kv.begin(), kv.end()};
    }

    DistributionPlatform &Platforms::getPlatform(const std::string &platform) {
        const auto plat = platforms_.find(platform);

        if (plat == platforms_.end()) {
            throw std::runtime_error(std::format("Platform '{}' not found", platform));
        }

        return plat->second;
    }
}
