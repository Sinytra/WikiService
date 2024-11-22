#include "documentation.h"

#include <drogon/drogon.h>

#include <map>

#include <trantor/net/EventLoopThreadPool.h>

#define DOCS_FILE_EXT ".mdx"
#define META_FILE_PATH "/_meta.json"
#define I18N_FILE_PATH "/.translated"
#define TYPE_FILE "file"
#define TYPE_DIR "dir"
#define BASE64_PREFIX "data:image/png;base64,"

using namespace logging;
using namespace drogon;
using namespace drogon::orm;
using namespace std::chrono_literals;

struct FolderMetadataEntry {
    const std::string name;
    const std::string icon;
};
struct FolderMetadata {
    std::vector<std::string> keys;
    std::map<std::string, FolderMetadataEntry> entries;
};

std::string createLocalesCacheKey(const std::string &id) { return "project:" + id + ":locales"; }

std::string createDocsTreeCacheKey(const std::string &id, const std::string &version, const std::optional<std::string> &locale) {
    return std::format("project:{}:docs_tree:{}:{}", id, version, locale.value_or("default"));
}

std::string createVersionsCacheKey(const std::string &id) { return "project:" + id + ":versions"; }

std::string getTreeEntryName(std::string s) {
    if (s.ends_with(DOCS_FILE_EXT)) {
        s = s.substr(0, s.size() - 4);
    }
    return toCamelCase(s);
}

std::string getTreeEntryPath(const std::string &s, size_t pathPrefix) {
    const auto relative = s.substr(std::min(s.size(), pathPrefix));
    return relative.ends_with(DOCS_FILE_EXT) ? relative.substr(0, relative.size() - 4) : relative;
}

Task<FolderMetadata> getFolderMetadata(service::GitHub &github, std::string repo, std::string version, std::string rootPath,
                                       std::string path, std::optional<std::string> locale, std::string installationToken) {
    FolderMetadata metadata;
    const auto filePath = rootPath + (locale ? "/.translated/" + *locale : "") + path + META_FILE_PATH;
    if (const auto [contents, contentsError](co_await github.getRepositoryContents(repo, version, filePath, installationToken)); contents) {
        const auto body = decodeBase64((*contents)["content"].asString());
        if (const auto parsed = tryParseJson<nlohmann::ordered_json>(body); parsed->is_object()) {
            for (auto &[key, val]: parsed->items()) {
                metadata.keys.push_back(key);
                if (val.is_string()) {
                    metadata.entries.try_emplace(key, val, "");
                } else if (val.is_object()) {
                    metadata.entries.try_emplace(key, val.contains("name") ? val["name"].get<std::string>() : getTreeEntryName(key),
                                                 val.value("icon", ""));
                }
            }
        }
    } else if (locale) {
        co_return co_await getFolderMetadata(github, repo, version, rootPath, path, std::nullopt, installationToken);
    }
    co_return metadata;
}

bool isValidEntry(const nlohmann::json &value) {
    return
        // Check structure
        value.is_object() && value.contains("type") && value.contains("name") &&
        // Exclude "hidden" files
        !value["name"].get<std::string>().starts_with('.') && !value["name"].get<std::string>().starts_with('_') &&
        // Filter our non-docs
        (value["type"] != TYPE_FILE || value["name"].get<std::string>().ends_with(DOCS_FILE_EXT));
}

auto createComparator(const std::vector<std::string>& keys) {
    return [&keys](const nlohmann::ordered_json &a, const nlohmann::ordered_json &b) {
        if (keys.empty()) {
            return a["name"].get<std::string>() < b["name"].get<std::string>();
        }
        const auto aPos = std::find(keys.begin(), keys.end(), a["name"]);
        const auto bPos = std::find(keys.begin(), keys.end(), b["name"]);
        if (aPos == keys.end() || bPos == keys.end()) {
            return false;
        }
        const auto aIdx = std::distance(keys.begin(), aPos);
        const auto bIdx = std::distance(keys.begin(), bPos);
        return aIdx < bIdx;
    };
}

Task<nlohmann::ordered_json> crawlDocsTree(service::GitHub &github, std::string repo, std::string version, std::string rootPath,
                                           std::string path, std::optional<std::string> locale, std::string installationToken,
                                           trantor::EventLoopThreadPool &pool) {
    auto currentLoop = trantor::EventLoop::getEventLoopOfCurrentThread();
    co_await switchThreadCoro(pool.getNextLoop());

    nlohmann::ordered_json root(nlohmann::json::value_t::array);
    if (const auto [contents, contentsError](co_await github.getRepositoryContents(repo, version, rootPath + path, installationToken));
        contents && contents->isArray())
    {
        auto json = parkourJson(*contents);
        json.erase(std::ranges::remove_if(json, [](const nlohmann::json &val) { return !isValidEntry(val); }).begin(), json.end());

        auto [keys, entries] = co_await getFolderMetadata(github, repo, version, rootPath, path, locale, installationToken);
        std::sort(json.begin(), json.end(), createComparator(keys));

        for (const auto &child: json) {
            const std::string childName(child["name"]);
            const auto [name, icon] =
                entries.contains(childName) ? entries[childName] : FolderMetadataEntry{getTreeEntryName(childName), ""};
            const auto relativePath = getTreeEntryPath(child["path"], rootPath.size());
            nlohmann::ordered_json obj;
            obj["name"] = name;
            if (!icon.empty()) {
                obj["icon"] = icon;
            }
            obj["path"] = relativePath;
            obj["type"] = child["type"];
            if (child["type"] == TYPE_DIR) {
                nlohmann::ordered_json children =
                    co_await crawlDocsTree(github, repo, version, rootPath, "/" + relativePath, locale, installationToken, pool);
                obj["children"] = children;
            }
            root.push_back(obj);
        }
    }
    co_await switchThreadCoro(currentLoop);
    co_return root;
}

namespace service {
    Documentation::Documentation(GitHub &g, MemoryCache &c) : github_(g), cache_(c) {}

    Task<std::tuple<bool, Error>> Documentation::hasAvailableLocale(const Project &project, std::string locale,
                                                                    std::string installationToken) {
        const auto cacheKey = createLocalesCacheKey(project.getValueOfId());

        if (const auto exists = co_await cache_.exists(cacheKey); !exists) {
            if (const auto pending = co_await getOrStartTask<std::tuple<bool, Error>>(cacheKey)) {
                co_return pending->get();
            }

            const auto [locales, localesError] = co_await computeAvailableLocales(project, installationToken);
            co_await cache_.updateCacheSet(cacheKey, *locales, 7 * 24h);

            const auto cached = co_await cache_.isSetMember(cacheKey, locale);
            co_return co_await completeTask<std::tuple<bool, Error>>(cacheKey, {cached, Error::Ok});
        }

        if (const auto cached = co_await cache_.isSetMember(cacheKey, locale)) {
            co_return {cached, Error::Ok};
        }

        co_return {false, Error::Ok};
    }

    Task<std::tuple<std::optional<Json::Value>, Error>> Documentation::getDocumentationPage(const Project &project, std::string path,
                                                                                            std::optional<std::string> locale,
                                                                                            std::string version,
                                                                                            std::string installationToken) {
        const auto sourcePath = removeTrailingSlash(project.getValueOfSourcePath());
        const auto resourcePath = sourcePath + (locale ? "/.translated/" + *locale : "") + "/" + removeLeadingSlash(path);
        const auto [contents, contentsError](
            co_await github_.getRepositoryContents(project.getValueOfSourceRepo(), version, resourcePath, installationToken));
        if (!contents && locale) {
            co_return co_await getDocumentationPage(project, path, std::nullopt, version, installationToken);
        }
        co_return {contents, contentsError};
    }

    Task<std::tuple<nlohmann::ordered_json, Error>> Documentation::getDirectoryTree(const Project &project, std::string version,
                                                                                    std::optional<std::string> locale,
                                                                                    std::string installationToken) {
        const auto cacheKey = createDocsTreeCacheKey(project.getValueOfId(), version, locale);

        if (const auto cached = co_await cache_.getFromCache(cacheKey)) {
            if (const auto parsed = tryParseJson<nlohmann::ordered_json>(*cached)) {
                co_return {*parsed, Error::Ok};
            }
        }

        if (const auto pending = co_await getOrStartTask<std::tuple<nlohmann::ordered_json, Error>>(cacheKey)) {
            co_return pending->get();
        }

        trantor::EventLoopThreadPool pool{10};
        pool.start();

        nlohmann::ordered_json root =
            co_await crawlDocsTree(github_, project.getValueOfSourceRepo(), version, removeTrailingSlash(project.getValueOfSourcePath()),
                                   "", locale, installationToken, pool);

        for (int16_t i = 0; i < 10; i++)
            pool.getLoop(i)->quit();
        pool.wait();

        co_await cache_.updateCache(cacheKey, root.dump(), 7 * 24h);
        co_return co_await completeTask<std::tuple<nlohmann::ordered_json, Error>>(cacheKey, {root, Error::Ok});
    }

    Task<std::tuple<std::optional<std::vector<std::string>>, Error>> Documentation::computeAvailableLocales(const Project &project,
                                                                                                            std::string installationToken) {
        std::vector<std::string> locales;

        const auto repo = project.getValueOfSourceRepo();
        const auto path = project.getValueOfSourcePath() + I18N_FILE_PATH;
        if (const auto [contents, contentsError](
                co_await github_.getRepositoryContents(repo, project.getValueOfSourceBranch(), path, installationToken));
            contents && contents->isArray())
        {
            for (const auto &child: *contents) {
                if (child.isObject() && child.isMember("type") && child["type"] == TYPE_DIR && child.isMember("name")) {
                    locales.push_back(child["name"].asString());
                }
            }
        }

        co_return {locales, Error::Ok};
    }

    Task<std::tuple<std::optional<std::string>, Error>>
    Documentation::getAssetResource(const Project &project, ResourceLocation location, std::string version, std::string installationToken) {
        const auto path = project.getValueOfSourcePath() + "/.assets/item/" + location.namespace_ + "/" + location.path_ +
                          (location.path_.find('.') != std::string::npos ? "" : ".png");

        const auto [isPublic, publicError](co_await github_.isPublicRepository(project.getValueOfSourceRepo(), installationToken));
        // Fast (static) track
        if (isPublic) {
            const auto baseUrl = std::format("https://raw.githubusercontent.com/{}/{}", project.getValueOfSourceRepo(), version);
            co_return {baseUrl + path, Error::Ok};
        }

        const auto [contents, contentsError](co_await github_.getRepositoryContents(
            project.getValueOfSourceRepo(), project.getValueOfSourceBranch(), path, installationToken));
        if (!contents) {
            co_return {std::nullopt, Error::ErrBadRequest};
        }
        co_return {BASE64_PREFIX + (*contents)["content"].asString(), Error::Ok};
    }

    Task<std::tuple<std::optional<Json::Value>, Error>> Documentation::getAvailableVersionsFiltered(const Project &project,
                                                                                                    std::string installationToken) {
        const auto [value, valueError] = co_await getAvailableVersions(project, installationToken);
        if (value) {
            co_return {value->empty() ? std::nullopt : value, valueError};
        }
        co_return {std::nullopt, Error::Ok};
    }

    Task<std::tuple<std::optional<Json::Value>, Error>> Documentation::getAvailableVersions(const Project &project,
                                                                                            std::string installationToken) {
        const auto cacheKey = createVersionsCacheKey(project.getValueOfId());

        if (const auto exists = co_await cache_.exists(cacheKey); !exists) {
            if (const auto pending = co_await getOrStartTask<std::tuple<std::optional<Json::Value>, Error>>(cacheKey)) {
                co_return pending->get();
            }

            const auto [versions, versionsError] = co_await computeAvailableVersions(project, installationToken);
            co_await cache_.updateCache(cacheKey, serializeJsonString(versions), 7 * 24h);

            co_return co_await completeTask<std::tuple<std::optional<Json::Value>, Error>>(cacheKey, {versions, versionsError});
        }

        if (const auto cached = co_await cache_.getFromCache(cacheKey)) {
            if (const auto parsed = parseJsonString(*cached)) {
                co_return {parsed, Error::Ok};
            }
        }

        co_return {std::nullopt, Error::Ok};
    }

    Task<std::tuple<Json::Value, Error>> Documentation::computeAvailableVersions(const Project &project, std::string installationToken) {
        const auto path = project.getValueOfSourcePath() + DOCS_META_FILE_PATH;
        const auto [contents, contentsError](co_await github_.getRepositoryJSONFile(
            project.getValueOfSourceRepo(), project.getValueOfSourceBranch(), path, installationToken));
        if (contents && contents->isObject() && contents->isMember("versions")) {
            Json::Value versions = (*contents)["versions"];
            co_return {versions, Error::Ok};
        }

        co_return {Json::Value(Json::objectValue), contentsError};
    }

    Task<> Documentation::invalidateCache(const Project &project) { co_await cache_.eraseNamespace("project:" + project.getValueOfId()); }
}
