#include "documentation.h"
#include "log/log.h"

#include <drogon/drogon.h>

using namespace logging;
using namespace drogon;
using namespace drogon::orm;
using namespace std::chrono_literals;

namespace service {
    Documentation::Documentation(service::GitHub &g, service::MemoryCache &c): github_(g), cache_(c) {
    }

    Task<std::tuple<bool, Error>> Documentation::hasAvailableLocale(const Mod& mod, std::string locale, std::string installationToken) {
        const auto cacheKey = "locales:" + *mod.getId();
        if (const auto cached = co_await cache_.isSetMember(cacheKey, locale)) {
            logger.debug("Reusing cached locale for mod {}", *mod.getId());
            co_return {cached, Error::Ok};
        }

        const auto [locales, localesError] = co_await getAvailableLocales(mod, installationToken);
        if (locales) {
            logger.debug("Storing available locales for {} in cache", *mod.getId());
            co_await cache_.updateCacheSet(cacheKey, *locales, 7 * 24h);
        }

        co_return {false, Error::Ok};
    }

    Task<std::tuple<std::optional<std::vector<std::string>>, Error>> Documentation::getAvailableLocales(
        const Mod &mod, const std::string installationToken
    ) {
        std::vector<std::string> locales;

        const auto repo = *mod.getSourceRepo();
        const auto path = *mod.getSourcePath() + "/.translated";
        if (const auto [contents, contentsError](
                co_await github_.getRepositoryContents(repo, std::nullopt, path, installationToken));
            contents && contents->isArray()) {
            for (const auto &child: *contents) {
                if (child.isObject() && child.isMember("type") && child["type"] == "dir" && child.isMember("name")) {
                    locales.push_back(child["name"].asString());
                }
            }
        }

        co_return {std::optional(locales), Error::Ok};
    }
}
