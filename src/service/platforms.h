#pragma once

#define PLATFORM_MODRINTH "modrinth"
#define PLATFORM_CURSEFORGE "curseforge"

#define WIKI_USER_AGENT "Sinytra/modded-wiki/1.0.0"
#define MODRINTH_API_URL "https://api.modrinth.com/"

#include <drogon/utils/coroutine.h>
#include <map>
#include <models/User.h>
#include <optional>
#include <string>

using namespace drogon_model::postgres;

namespace service {
    enum ProjectType {
        mod,
        resourcepack,
        datapack,
        shader,
        modpack,
        plugin,

        _unknown
    };

    std::string projectTypeToString(const ProjectType &type);

    struct PlatformProject {
        std::string slug;
        std::string name;
        std::string sourceUrl;
        ProjectType type;
        std::string platform;
    };

    class DistributionPlatform {
    public:
        virtual drogon::Task<std::optional<PlatformProject>> getProject(std::string slug) = 0;

        virtual drogon::Task<bool> verifyProjectAccess(PlatformProject project, User user, std::string repo);
    };

    class ModrinthPlatform : public DistributionPlatform {
    public:
        explicit ModrinthPlatform();

        drogon::Task<std::optional<std::string>> getAuthenticatedUserID(std::string token) const;
        drogon::Task<bool> isProjectMember(std::string slug, std::string userId) const;

        drogon::Task<std::optional<PlatformProject>> getProject(std::string slug) override;

        drogon::Task<bool> verifyProjectAccess(PlatformProject project, User user, std::string repo) override;
    };

    class CurseForgePlatform : public DistributionPlatform {
    public:
        explicit CurseForgePlatform(std::string);

        bool isAvailable() const;

        drogon::Task<std::optional<PlatformProject>> getProject(std::string slug) override;
    private:
        const std::string apiKey_;
    };

    class Platforms {
    public:
        explicit Platforms(CurseForgePlatform&, ModrinthPlatform&);

        drogon::Task<std::optional<PlatformProject>> getProject(std::string platform, std::string slug);

        std::vector<std::string> getAvailablePlatforms();
        DistributionPlatform& getPlatform(const std::string &platform);

        CurseForgePlatform& curseforge_;
        ModrinthPlatform& modrinth_;
    private:
        std::map<std::string, DistributionPlatform&> platforms_;
    };
}
