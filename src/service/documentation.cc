#include "documentation.h"

#include <drogon/drogon.h>

#include <map>
#include <mutex>

#include <trantor/net/EventLoopThreadPool.h>

#define DOCS_FILE_EXT ".mdx"
#define META_FILE_PATH "/_meta.json"
#define DOCS_META_FILE_PATH "/sinytra-wiki.json"
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
using FolderMetadata = std::map<std::string, FolderMetadataEntry>;

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
        if (const auto parsed = parseJsonString(body); parsed->isObject()) {
            for (auto it = parsed->begin(); it != parsed->end(); it++) {
                if (it->isString()) {
                    metadata.try_emplace(it.key().asString(), it->asString(), "");
                } else if (it->isObject()) {
                    metadata.try_emplace(it.key().asString(),
                                         it->isMember("name") ? (*it)["name"].asString() : getTreeEntryName(it.key().asString()),
                                         it->get("icon", "").asString());
                }
            }
        }
    } else if (locale) {
        co_return co_await getFolderMetadata(github, repo, version, rootPath, path, std::nullopt, installationToken);
    }
    co_return metadata;
}

bool isValidEntry(const Json::Value &value) {
    return
        // Check structure
        value.isObject() && value.isMember("type") && value.isMember("name") &&
        // Exclude "hidden" files
        !value["name"].asString().starts_with('.') && !value["name"].asString().starts_with('_') &&
        // Filter our non-docs
        (value["type"] != TYPE_FILE || value["name"].asString().ends_with(DOCS_FILE_EXT));
}

Task<Json::Value> crawlDocsTree(service::GitHub &github, std::string repo, std::string version, std::string rootPath, std::string path,
                                std::optional<std::string> locale, std::string installationToken, trantor::EventLoopThreadPool &pool) {
    auto currentLoop = trantor::EventLoop::getEventLoopOfCurrentThread();
    co_await switchThreadCoro(pool.getNextLoop());

    Json::Value root{Json::arrayValue};
    if (const auto [contents, contentsError](co_await github.getRepositoryContents(repo, version, rootPath + path, installationToken));
        contents && contents->isArray())
    {
        FolderMetadata metadata = co_await getFolderMetadata(github, repo, version, rootPath, path, locale, installationToken);

        for (const auto &child: *contents) {
            if (isValidEntry(child)) {
                const auto childName = child["name"].asString();
                const auto childMeta =
                    metadata.contains(childName) ? metadata[childName] : FolderMetadataEntry{getTreeEntryName(childName), ""};
                const auto relativePath = getTreeEntryPath(child["path"].asString(), rootPath.size());
                Json::Value obj;
                obj["name"] = childMeta.name;
                if (!childMeta.icon.empty()) {
                    obj["icon"] = childMeta.icon;
                }
                obj["path"] = relativePath;
                obj["type"] = child["type"];
                if (child["type"] == TYPE_DIR) {
                    Json::Value children =
                        co_await crawlDocsTree(github, repo, version, rootPath, "/" + relativePath, locale, installationToken, pool);
                    obj["children"] = children;
                }
                root.append(obj);
            }
        }
    }
    co_await switchThreadCoro(currentLoop);
    co_return root;
}

namespace service {
    Documentation::Documentation(service::GitHub &g, service::MemoryCache &c) : github_(g), cache_(c) {}

    Task<std::tuple<bool, Error>> Documentation::hasAvailableLocale(const Project &project, std::string locale,
                                                                    std::string installationToken) {
        const auto cacheKey = createLocalesCacheKey(project.getValueOfId());

        if (const auto exists = co_await cache_.exists(cacheKey); !exists) {
            if (const auto pending = co_await CacheableServiceBase::getOrStartTask<std::tuple<bool, Error>>(cacheKey)) {
                co_return pending->get();
            }

            const auto [locales, localesError] = co_await computeAvailableLocales(project, installationToken);
            co_await cache_.updateCacheSet(cacheKey, *locales, 7 * 24h);

            const auto cached = co_await cache_.isSetMember(cacheKey, locale);
            co_return co_await CacheableServiceBase::completeTask<std::tuple<bool, Error>>(cacheKey, {cached, Error::Ok});
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

    Task<std::tuple<Json::Value, Error>> Documentation::getDirectoryTree(const Project &project, std::string version,
                                                                         std::optional<std::string> locale, std::string installationToken) {
        const auto cacheKey = createDocsTreeCacheKey(project.getValueOfId(), version, locale);

        if (const auto cached = co_await cache_.getFromCache(cacheKey)) {
            if (const auto parsed = parseJsonString(*cached)) {
                co_return {*parsed, Error::Ok};
            }
        }

        if (const auto pending = co_await CacheableServiceBase::getOrStartTask<std::tuple<Json::Value, Error>>(cacheKey)) {
            co_return pending->get();
        }

        trantor::EventLoopThreadPool pool{10};
        pool.start();

        Json::Value root = co_await crawlDocsTree(github_, project.getValueOfSourceRepo(), version,
                                                  removeTrailingSlash(project.getValueOfSourcePath()), "", locale, installationToken, pool);

        for (int16_t i = 0; i < 10; i++)
            pool.getLoop(i)->quit();
        pool.wait();

        co_await cache_.updateCache(cacheKey, serializeJsonString(root), 7 * 24h);
        co_return co_await CacheableServiceBase::completeTask<std::tuple<Json::Value, Error>>(cacheKey, {root, Error::Ok});
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
            if (const auto pending = co_await CacheableServiceBase::getOrStartTask<std::tuple<std::optional<Json::Value>, Error>>(cacheKey))
            {
                co_return pending->get();
            }

            const auto [versions, versionsError] = co_await computeAvailableVersions(project, installationToken);
            co_await cache_.updateCache(cacheKey, serializeJsonString(versions), 7 * 24h);

            co_return co_await CacheableServiceBase::completeTask<std::tuple<std::optional<Json::Value>, Error>>(cacheKey,
                                                                                                                 {versions, versionsError});
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
