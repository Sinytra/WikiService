#include "resolved.h"

#include <service/database/database.h>
#include <service/database/resolved_db.h>
#include <service/project/git_hosts.h>
#include <service/project/properties.h>
#include <service/schemas.h>
#include <service/storage/gitops.h>
#include <service/util.h>

#include <filesystem>
#include <unordered_map>

#include <content/recipe_resolver.h>
#include <fmt/args.h>
#include <models/Item.h>
#include <models/ProjectItem.h>

using namespace logging;
using namespace drogon;
using namespace drogon_model::postgres;
namespace fs = std::filesystem;

namespace service {
    std::string formatCommitUrl(const Project &project, const std::string &hash) {
        const auto provider = getGitProvider(project.getValueOfSourceRepo());
        if (!provider) {
            return "";
        }

        fmt::dynamic_format_arg_store<fmt::format_context> store;
        store.push_back(fmt::arg("hash", hash));
        const auto result = vformat(provider->commitPath, store);

        return removeTrailingSlash(project.getValueOfSourceRepo()) + "/" + result;
    }

    ResolvedProject::ResolvedProject(const Project &p, const std::filesystem::path &d, const ProjectVersion &v) :
        project_(p), defaultVersion_(nullptr), docsDir_(d), version_(v), projectDb_(std::make_shared<ProjectDatabaseAccess>(*this)),
        format_(ProjectFormat{d, ""}) {}

    void ResolvedProject::setDefaultVersion(const ResolvedProject &defaultVersion) {
        defaultVersion_ = std::make_shared<ResolvedProject>(defaultVersion);
    }

    void ResolvedProject::setLocale(const std::optional<std::string> &locale) { format_.setLocale(locale); }
    std::string ResolvedProject::getLocale() const { return format_.getLocale(); }

    bool ResolvedProject::hasLocale(const std::string &locale) const {
        const auto locales = getLocales();
        return locales.contains(locale);
    }

    std::set<std::string> ResolvedProject::getLocales() const {
        std::set<std::string> locales;

        if (const auto localesPath = format_.getLocalesPath(); exists(localesPath) && is_directory(localesPath)) {
            for (const auto &entry: fs::directory_iterator(localesPath)) {
                if (entry.is_directory()) {
                    locales.insert(entry.path().filename().string());
                }
            }
        }

        return locales;
    }

    Task<std::unordered_map<std::string, std::string>> ResolvedProject::getAvailableVersions() const {
        std::unordered_map<std::string, std::string> versions;

        for (const auto dbVersions = co_await projectDb_->getVersions(); const auto &version: dbVersions) {
            versions.emplace(version.getValueOfName(), version.getValueOfBranch());
        }

        co_return versions;
    }

    Task<bool> ResolvedProject::hasVersion(const std::string version) const {
        const auto versions = co_await getAvailableVersions();
        co_return versions.contains(version);
    }

    std::optional<std::filesystem::path> ResolvedProject::getAsset(const ResourceLocation &location) const {
        if (const auto filePath = format_.getAssetPath(location); exists(filePath)) {
            return filePath;
        }

        // Legacy asset path fallback
        if (const auto legacyFilePath = format_.getAssetPath({.namespace_ = "item", .path_ = location.namespace_ + '/' + location.path_});
            exists(legacyFilePath))
        {
            return legacyFilePath;
        }

        return std::nullopt;
    }

    const Project &ResolvedProject::getProject() const { return project_; }

    const ProjectVersion &ResolvedProject::getProjectVersion() const { return version_; }

    const std::filesystem::path &ResolvedProject::getRootDirectory() const { return docsDir_; }

    const ProjectFormat &ResolvedProject::getFormat() const { return format_; };

    ProjectDatabaseAccess &ResolvedProject::getProjectDatabase() const { return *projectDb_; }

    Task<Json::Value> ResolvedProject::toJson(const bool full) const {
        const auto versions = co_await getAvailableVersions();
        const auto locales = getLocales();

        Json::Value projectJson = projectToJson(project_, full);

        if (!versions.empty()) {
            Json::Value versionsJson(Json::arrayValue);
            for (const auto &key: versions | std::views::keys) {
                versionsJson.append(key);
            }
            projectJson["versions"] = versionsJson;
        }

        if (!locales.empty()) {
            Json::Value localesJson(Json::arrayValue);
            for (const auto &item: locales) {
                localesJson.append(item);
            }
            projectJson["locales"] = localesJson;
        }

        if (full) {
            if (const auto latestDeployment = co_await global::database->getActiveDeployment(project_.getValueOfId())) {
                if (const auto revisionStr = latestDeployment->getRevision()) {
                    projectJson["revision"] = parseJsonOrThrow(*revisionStr);

                    const git::GitRevision revision = nlohmann::json::parse(*revisionStr);
                    if (const auto url = formatCommitUrl(project_, revision.fullHash); !url.empty()) {
                        projectJson["revision"]["url"] = url;
                    }
                }
            }

            const auto issueStats = co_await global::database->getActiveProjectIssueStats(project_.getValueOfId());
            projectJson["issue_stats"] = unparkourJson(nlohmann::json(issueStats));

            const auto hasFailingDeployment = co_await global::database->hasFailingDeployment(project_.getValueOfId());
            projectJson["has_failing_deployment"] = hasFailingDeployment;
        }

        co_return projectJson;
    }

    int countPagesRecursive(const FileTree &tree) {
        int sum = 0;

        for (const auto &entry: tree) {
            if (entry.type == FileType::DIR) {
                sum += countPagesRecursive(entry.children);
            } else if (entry.type == FileType::FILE) {
                sum++;
            }
        }

        return sum;
    }

    Task<Json::Value> ResolvedProject::toJsonVerbose() const {
        auto projectJson = co_await toJson();
        Json::Value infoJson;

        if (const auto [meta, err, detail] = validateProjectMetadata(); meta && meta->contains("links")) {
            if (const auto links = (*meta)["links"]; links.contains("website")) {
                infoJson["website"] = links["website"].get<std::string>();
            }
        }

        const auto count = co_await projectDb_->getProjectContentCount();
        infoJson["contentCount"] = count;

        const auto [tree, treeErr] = getDirectoryTree();
        infoJson["pageCount"] = treeErr == Error::Ok ? countPagesRecursive(tree) : 0;

        projectJson["info"] = infoJson;
        co_return projectJson;
    }

    std::tuple<std::optional<nlohmann::json>, ProjectError, std::string> ResolvedProject::validateProjectMetadata() const {
        const auto path = format_.getWikiMetadataPath();
        if (!exists(path)) {
            return {std::nullopt, ProjectError::NO_PATH, ""};
        }

        const auto meta = parseJsonFile(path);
        if (!meta) {
            return {std::nullopt, ProjectError::INVALID_META, ""};
        }

        if (const auto error = validateJson(schemas::projectMetadata, *meta)) {
            auto field = error->pointer.to_string();
            if (field.starts_with('/')) {
                field = field.substr(1);
            }
            const auto detail = field + ": " + error->msg;
            return {std::nullopt, ProjectError::INVALID_META, detail};
        }

        return {*meta, ProjectError::OK, ""};
    }

    Task<PaginatedData<FullItemData>> ResolvedProject::getItems(const TableQueryParams params) const {
        const auto [total, pages, size, data] = co_await projectDb_->getProjectItemsDev(params.query, params.page);
        std::vector<FullItemData> itemData;
        for (const auto &[id, path]: data) {
            const auto [name, _] = co_await getItemName(id);
            itemData.emplace_back(id, name, path);
        }
        co_return PaginatedData{.total = total, .pages = pages, .size = size, .data = itemData};
    }

    Task<PaginatedData<FullTagData>> ResolvedProject::getTags(TableQueryParams params) const {
        const auto [total, pages, size, data] = co_await projectDb_->getProjectTagsDev(params.query, params.page);
        std::vector<FullTagData> tagData;
        for (const auto &[id, loc]: data) {
            const auto tagItems = co_await projectDb_->getProjectTagItemsFlat(id);
            std::vector<std::string> items;
            for (auto &item: tagItems) {
                items.push_back(item.getValueOfLoc());
            }
            tagData.emplace_back(loc, items);
        }
        co_return PaginatedData{.total = total, .pages = pages, .size = size, .data = tagData};
    }

    Task<PaginatedData<FullItemData>> ResolvedProject::getTagItems(const std::string tag, const TableQueryParams params) const {
        const auto [total, pages, size, data] = co_await projectDb_->getProjectTagItemsDev(tag, params.query, params.page);
        std::vector<FullItemData> itemData;
        for (const auto &[id, path]: data) {
            const auto [name, _] = co_await getItemName(id);
            itemData.emplace_back(id, name, path);
        }
        co_return PaginatedData{.total = total, .pages = pages, .size = size, .data = itemData};
    }

    Task<PaginatedData<FullRecipeData>> ResolvedProject::getRecipes(const TableQueryParams params) const {
        const auto [total, pages, size, data] = co_await projectDb_->getProjectRecipesDev(params.query, params.page);
        std::vector<FullRecipeData> recipeDate;
        for (const auto &recipe: data) {
            const auto resolved = co_await content::resolveRecipe(recipe, format_.getLocale());
            recipeDate.emplace_back(recipe.getValueOfLoc(), resolved ? nlohmann::json(*resolved) : nlohmann::json{});
        }
        co_return PaginatedData{.total = total, .pages = pages, .size = size, .data = recipeDate};
    }

    Task<PaginatedData<ProjectVersion>> ResolvedProject::getVersions(const TableQueryParams params) const {
        co_return co_await projectDb_->getVersionsDev(params.query, params.page);
    }
}
