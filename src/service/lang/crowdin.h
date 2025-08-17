#pragma once

#include <config.h>
#include <drogon/utils/coroutine.h>
#include <service/cache.h>
#include <vector>
#include <nlohmann/json.hpp>

namespace service {
    struct Locale {
        std::string id;
        std::string name;
        std::string code;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Locale, id, name, code)
    };

    class Crowdin : public CacheableServiceBase {
    public:
        explicit Crowdin(const config::Crowdin &);

        drogon::Task<std::vector<Locale>> getAvailableLocales();
        drogon::Task<bool> hasLocaleKey(std::string locale);
        drogon::Task<> reloadAvailableLocales();
    private:
        drogon::Task<std::vector<Locale>> computeLanguages() const;

        const config::Crowdin &config_;
    };
}

namespace global {
    extern std::shared_ptr<service::Crowdin> crowdin;
}