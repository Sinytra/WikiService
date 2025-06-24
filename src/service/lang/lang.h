#pragma once

#include <service/cache.h>
#include <drogon/utils/coroutine.h>

#include <service/error.h>

namespace service {
    class LangService : public CacheableServiceBase {
    public:
        explicit LangService();

        drogon::Task<std::string> getItemName(std::optional<std::string> lang, std::string location);
    private:
        drogon::Task<Error> loadItemLanguageKeys(std::string lang);
    };
}

namespace global {
    extern std::shared_ptr<service::LangService> lang;
}