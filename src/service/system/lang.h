#pragma once

#include <service/external/crowdin.h>
#include <drogon/utils/coroutine.h>
#include <service/cache.h>
#include <service/error.h>

#define DEFAULT_LOCALE "en_us"

namespace service {
    drogon::Task<std::optional<std::string>> validateLocale(std::optional<std::string> locale);

    class LangService : public CacheableServiceBase {
    public:
        explicit LangService();

        drogon::Task<std::vector<Locale>> getAvailableLocales() const;

        drogon::Task<std::optional<std::string>> getItemName(std::optional<std::string> lang, std::string location);
    private:
        drogon::Task<Error> loadItemLanguageKeys(std::string lang);
    };
}

namespace global {
    extern std::shared_ptr<service::LangService> lang;
}