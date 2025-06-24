#pragma once

#include <drogon/utils/coroutine.h>
#include <filesystem>
#include <nlohmann/json_fwd.hpp>
#include <service/cache.h>

namespace service {
    class GameDataService : public CacheableServiceBase {
    public:
        explicit GameDataService(const std::string &);

        drogon::Task<> setupGameData() const;

        std::optional<nlohmann::json> getLang(const std::string &name) const;
    private:
        drogon::Task<> downloadAssets(std::filesystem::path langDir) const;
        void extractClient(const std::filesystem::path &clientPath, std::filesystem::path langDir) const;
        drogon::Task<nlohmann::json> bufferJsonFile(std::string url) const;
        drogon::Task<> downloadFile(std::string url, std::filesystem::path dest) const;

        const std::filesystem::path baseDir_;
    };
}

namespace global {
    extern std::shared_ptr<service::GameDataService> gameData;
}