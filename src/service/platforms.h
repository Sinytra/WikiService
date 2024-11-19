#pragma once

#include "error.h"

#include <drogon/utils/coroutine.h>
#include <json/value.h>
#include <optional>
#include <string>
#include <tuple>

namespace service {
    struct PlatformProject {
        std::string slug;
        std::string sourceUrl;
    };

    class DistributionPlatform {
    public:
        virtual drogon::Task<std::optional<PlatformProject>> getProject(std::string slug) = 0;
    };

    class ModrinthPlatform : public DistributionPlatform {
    public:
        virtual drogon::Task<std::optional<PlatformProject>> getProject(std::string slug) override;
    };

    class CurseForgePlatform : public DistributionPlatform {
    public:
        explicit CurseForgePlatform(std::string apiKey);

        virtual drogon::Task<std::optional<PlatformProject>> getProject(std::string slug) override;
    private:
        const std::string apiKey_;
    };

    class Platforms {
    public:
        explicit Platforms(const std::map<std::string, DistributionPlatform&>&);

        drogon::Task<std::optional<PlatformProject>> getProject(std::string platform, std::string slug);
    private:
        std::map<std::string, DistributionPlatform&> platforms_;
    };
}
