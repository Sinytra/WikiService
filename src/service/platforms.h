#pragma once

#define PLATFORM_MODRINTH "modrinth"
#define PLATFORM_CURSEFORGE "curseforge"

#include <drogon/utils/coroutine.h>
#include <json/value.h>
#include <optional>
#include <string>

namespace service {
    struct PlatformProject {
        std::string slug;
        std::string name;
        std::string sourceUrl;
    };

    class DistributionPlatform {
    public:
        virtual drogon::Task<std::optional<PlatformProject>> getProject(std::string slug) = 0;
    };

    class ModrinthPlatform : public DistributionPlatform {
    public:
        explicit ModrinthPlatform(const std::string &, const std::string &, const std::string &);

        bool isOAuthConfigured() const;

        drogon::Task<std::optional<std::string>> getOAuthToken(std::string code) const;
        drogon::Task<std::optional<std::string>> getAuthenticatedUser(std::string token) const;
        drogon::Task<bool> isProjectMember(std::string slug, std::string username) const;

        virtual drogon::Task<std::optional<PlatformProject>> getProject(std::string slug) override;
    private:
        const std::string clientId_;
        const std::string clientSecret_;
        const std::string redirectUrl_;
    };

    class CurseForgePlatform : public DistributionPlatform {
    public:
        explicit CurseForgePlatform(std::string);

        virtual drogon::Task<std::optional<PlatformProject>> getProject(std::string slug) override;
    private:
        const std::string apiKey_;
    };

    class Platforms {
    public:
        explicit Platforms(ModrinthPlatform&, const std::map<std::string, DistributionPlatform&>&);

        drogon::Task<std::optional<PlatformProject>> getProject(std::string platform, std::string slug);

        ModrinthPlatform& modrinth_;
    private:
        std::map<std::string, DistributionPlatform&> platforms_;
    };
}
