#include "lang.h"
#include "game_data.h"

#include <chrono>
#include <service/external/crowdin.h>
#include <service/util.h>

using namespace drogon;
using namespace std::chrono;
using namespace std::chrono_literals;

namespace service {
    const static std::unordered_map<std::string, std::string> mojangLanguages = {
        {"ms_arab", "zlm_arab"}
    };

    LangService::LangService() {}

    Task<std::optional<std::string>> validateLocale(std::optional<std::string> locale) {
        if (!locale || *locale == DEFAULT_LOCALE) {
            co_return std::nullopt;
        }
        if (!co_await global::crowdin->hasLocaleKey(*locale)) {
            throw ApiException(Error::ErrBadRequest, "Invalid locale " + *locale);
        }
        co_return mojangLanguages.contains(*locale) ? mojangLanguages.at(*locale) : *locale;
    }

    Task<std::vector<Locale>> LangService::getAvailableLocales() const {
        auto locales = co_await global::crowdin->getAvailableLocales();
        for (auto &locale : locales) {
            if (const auto mapped = mojangLanguages.find(locale.code); mapped != mojangLanguages.end()) {
                locale.code = mapped->second;
            }
        }
        co_return locales;
    }

    Task<std::optional<std::string>> LangService::getItemName(const std::optional<std::string> lang, const std::string location) {
        const std::string mcLang = lang.value_or(DEFAULT_LOCALE);

        if (const auto err = co_await loadItemLanguageKeys(mcLang); err != Error::Ok) {
            if (mcLang != DEFAULT_LOCALE && err == Error::ErrNotFound) {
                co_return co_await getItemName(std::nullopt, location);
            }
            co_return std::nullopt;
        }

        const auto loc = ResourceLocation::parse(location);
        if (!loc || loc->namespace_ != ResourceLocation::DEFAULT_NAMESPACE) {
            co_return std::nullopt;
        }

        const auto langCacheKey = std::format("lang:{}:minecraft:{}", mcLang, loc->path_);
        const auto cached = co_await global::cache->getFromCache(langCacheKey);

        co_return cached;
    }

    Task<Error> LangService::loadItemLanguageKeys(const std::string lang) {
        const auto cacheKey = "lang:" + lang;
        static const std::vector<std::string> prefixes = {"item.minecraft.", "block.minecraft."};

        if (const auto cached = co_await global::cache->getFromCache(cacheKey)) {
            co_return Error::Ok;
        }

        if (const auto pending = co_await getOrStartTask<Error>(cacheKey)) {
            co_return co_await patientlyAwaitTaskResult(*pending);
        }

        try {
            const auto langFile = global::gameData->getLang(lang);
            if (!langFile) {
                co_return co_await completeTask<Error>(cacheKey, Error::ErrNotFound);
            }

            for (const auto &[key, val]: langFile->items()) {
                for (const auto &prefix : prefixes) {
                    if (key.starts_with(prefix)) {
                        if (const auto subKey = key.substr(prefix.size()); subKey.find(".") == std::string::npos) {
                            const auto langCacheKey = std::format("lang:{}:minecraft:{}", lang, subKey);
                            co_await global::cache->updateCache(langCacheKey, val.get<std::string>(), 0s);
                        }
                        break;
                    }
                }
            }

            co_await global::cache->updateCache(cacheKey, "_", 0s);

            co_return co_await completeTask<Error>(cacheKey, Error::Ok);
        } catch (std::exception &e) {
            logging::logger.error("Error loading langues keys for {}: {}", lang, e.what());
        }

        co_return co_await completeTask<Error>(cacheKey, Error::ErrInternal);
    }
}
