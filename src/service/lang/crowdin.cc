#include "crowdin.h"

#include <chrono>
#include "util.h"

#define CROWDIN_API "https://api.crowdin.com"
#define CROWDIN_PROJECT "/api/v2/projects"

using namespace drogon;
using namespace logging;
using namespace std::chrono_literals;

namespace service {
    const static std::string localesCacheKey = "crowdin:languages";
    const static std::string langKeysCacheKey = "crowdin:languages:keys";

    Crowdin::Crowdin(const config::Crowdin &config) : config_(config) {}

    Task<bool> Crowdin::hasLocaleKey(const std::string locale) {
        if (!co_await global::cache->exists(langKeysCacheKey)) {
            co_await getAvailableLocales();
        }
        co_return co_await global::cache->isSetMember(langKeysCacheKey, locale);
    }

    Task<std::vector<Locale>> Crowdin::getAvailableLocales() {
        if (const auto cached = co_await global::cache->getFromCache(localesCacheKey)) {
            co_return nlohmann::json::parse(*cached);
        }

        if (const auto pending = co_await getOrStartTask<std::vector<Locale>>(localesCacheKey)) {
            co_return co_await patientlyAwaitTaskResult(*pending);
        }

        std::vector<Locale> locales;
        try {
            logger.info("Loading available languages from Crowdin");

            locales = co_await computeLanguages();

            logger.info("Loaded {} languages", locales.size());
        } catch (std::exception err) {
            logger.error("Error loading locale list from Crowdin: {}", err.what());
        }

        co_await global::cache->updateCache(localesCacheKey, nlohmann::json(locales).dump(), 7 * 24h);

        std::vector<std::string> keys;
        for (const auto &locale : locales)
            keys.push_back(locale.code);
        co_await global::cache->updateCacheSet(langKeysCacheKey, keys, 7 * 24h);

        co_return co_await completeTask<std::vector<Locale>>(localesCacheKey, locales);
    }

    Task<> Crowdin::reloadAvailableLocales() {
        co_await global::cache->erase(localesCacheKey);
        co_await getAvailableLocales();
    }

    Task<std::vector<Locale>> Crowdin::computeLanguages() const {
        const auto client = createHttpClient(CROWDIN_API);
        const auto httpReq = HttpRequest::newHttpRequest();
        httpReq->setMethod(Get);
        httpReq->setPath(std::format("{}/{}", CROWDIN_PROJECT, config_.projectId));
        httpReq->addHeader("Authorization", "Bearer " + config_.token);

        const auto response = co_await client->sendRequestCoro(httpReq);

        if (response->getStatusCode() != k200OK) {
            throw std::runtime_error("Unexpected response code: " + std::to_string(response->getStatusCode()));
        }

        if (const auto json = response->getJsonObject()) {
            const auto targetLanguages = (*json)["data"]["targetLanguages"];
            std::vector<Locale> locales;
            for (auto it = targetLanguages.begin(); it != targetLanguages.end(); ++it) {
                const auto id = (*it)["id"].asString();
                const auto name = (*it)["name"].asString();
                auto code = strToLower((*it)["locale"].asString());
                replaceAll(code, "-", "_");
                locales.emplace_back(id, name, code);
            }
            co_return locales;
        }

        throw std::runtime_error("Expected response body to be JSON");
    }
}
