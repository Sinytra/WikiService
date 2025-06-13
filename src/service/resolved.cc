#include "resolved.h"
#include "database/database.h"
#include "database/resolved_db.h"
#include "schemas.h"
#include "util.h"

#include <filesystem>
#include <fstream>
#include <unordered_map>

#include <content/game_recipes.h>
#include <fmt/args.h>
#include <include/uri.h>
#include <models/Item.h>
#include <models/ProjectItem.h>
#include <models/Tag.h>

#define DOCS_META_FILE "sinytra-wiki.json"
#define FOLDER_META_FILE "_meta.json"
#define DOCS_FILE_EXT ".mdx"
#define I18N_DIR_PATH ".translated"
#define NO_ICON "_none"
#define DEFAULT_LOCALE "en_en"

using namespace logging;
using namespace drogon;
using namespace drogon_model::postgres;
namespace fs = std::filesystem;

struct GitProvider {
    std::string filePath;
    std::string commitPath;
};

std::unordered_map<std::string, GitProvider> GIT_PROVIDERS = {
    {"github.com", GitProvider{.filePath = "blob/{branch}/{base}/{path}", .commitPath = "commit/{hash}"}}};

struct FolderMetadataEntry {
    const std::string name;
    const std::string icon;
};
struct FolderMetadata {
    std::vector<std::string> keys;
    std::map<std::string, FolderMetadataEntry> entries;
};

std::optional<GitProvider> getGitProvider(const std::string &url) {
    const uri parsed{url};
    const auto domain = parsed.get_host();

    const auto provider = GIT_PROVIDERS.find(domain);
    if (provider == GIT_PROVIDERS.end()) {
        return std::nullopt;
    }

    return provider->second;
}

std::string getDocsTreeEntryName(std::string s) {
    if (s.ends_with(DOCS_FILE_EXT)) {
        s = s.substr(0, s.size() - 4);
    }
    return toCamelCase(s);
}

std::string getDocsTreeEntryPath(const std::string &s) { return s.ends_with(DOCS_FILE_EXT) ? s.substr(0, s.size() - 4) : s; }

fs::path getFolderMetaFilePath(const fs::path &rootDir, const fs::path &dir, const std::string &locale) {
    if (!locale.empty()) {
        const auto relativePath = relative(dir, rootDir);

        if (const auto localeFile = rootDir / I18N_DIR_PATH / locale / relativePath / FOLDER_META_FILE; exists(localeFile)) {
            return localeFile;
        }
    }

    return dir / FOLDER_META_FILE;
}

FolderMetadata getFolderMetadata(const Project &project, const fs::path &path) {
    FolderMetadata metadata;
    if (!exists(path)) {
        return metadata;
    }

    std::ifstream ifs(path);
    try {
        nlohmann::ordered_json parsed = nlohmann::ordered_json::parse(ifs);
        if (const auto error = validateJson(schemas::folderMetadata, parsed)) {
            logger.error("Invalid folder metadata: Project: {} Path: {} Error: {}", project.getValueOfId(), path.string(), error->msg);
        } else {
            for (auto &[key, val]: parsed.items()) {
                metadata.keys.push_back(key);
                if (val.is_string()) {
                    metadata.entries.try_emplace(key, val, "");
                } else if (val.is_object()) {
                    metadata.entries.try_emplace(key, val.contains("name") ? val["name"].get<std::string>() : getDocsTreeEntryName(key),
                                                 val.contains("icon") ? (val["icon"].is_null() ? NO_ICON : val.value("icon", "")) : "");
                }
            }
        }
        ifs.close();
    } catch (const nlohmann::json::parse_error &e) {
        ifs.close();
        logger.error("Error parsing folder metadata: Project '{}', path {}", project.getValueOfId(), path.string());
        logger.error("JSON parse error (getFolderMetadata): {}", e.what());
    }
    return metadata;
}

auto createDirEntriesComparator(const std::vector<std::string> &keys) {
    return [&keys](const fs::directory_entry &a, const fs::directory_entry &b) {
        if (keys.empty()) {
            return a.path().filename().string() < b.path().filename().string();
        }
        const auto aPos = std::ranges::find(keys, a.path().filename().string());
        const auto bPos = std::ranges::find(keys, b.path().filename().string());
        if (aPos == keys.end() && bPos == keys.end()) {
            return false;
        }
        if (aPos == keys.end()) {
            return false;
        }
        if (bPos == keys.end()) {
            return true;
        }

        const auto aIdx = std::distance(keys.begin(), aPos);
        const auto bIdx = std::distance(keys.begin(), bPos);
        return aIdx < bIdx;
    };
}

nlohmann::ordered_json getDirTreeJson(const Project &project, const fs::path &rootDir, const fs::path &dir, const std::string &locale) {
    nlohmann::ordered_json root(nlohmann::json::value_t::array);

    const auto metaFilePath = getFolderMetaFilePath(rootDir, dir, locale);
    auto [keys, entries] = getFolderMetadata(project, metaFilePath);

    std::vector<fs::directory_entry> paths;
    for (const auto &entry: fs::directory_iterator(dir)) {
        if (const auto fileName = entry.path().filename().string();
            (entry.is_directory() || entry.is_regular_file() && fileName.ends_with(DOCS_FILE_EXT)) && !fileName.starts_with(".") &&
            !fileName.starts_with("_"))
        {
            paths.push_back(entry);
        }
    }
    std::ranges::sort(paths, createDirEntriesComparator(keys));

    for (const auto &entry: paths) {
        const auto fileName = entry.path().filename().string();
        const auto relativePath = relative(entry.path(), rootDir);

        const auto [name, icon] = entries.contains(fileName) ? entries[fileName] : FolderMetadataEntry{getDocsTreeEntryName(fileName), ""};
        nlohmann::ordered_json obj;
        obj["name"] = name;
        if (!icon.empty()) {
            obj["icon"] = icon;
        }
        obj["path"] = getDocsTreeEntryPath(relativePath);
        obj["type"] = entry.is_directory() ? "dir" : "file";
        if (entry.is_directory()) {
            nlohmann::ordered_json children = getDirTreeJson(project, rootDir, entry.path(), locale);
            obj["children"] = children;
        }
        root.push_back(obj);
    }

    return root;
}

Task<> addPageIDs(const std::unordered_map<std::string, std::string> &ids, nlohmann::ordered_json &json) {
    if (!json.is_array()) {
        co_return;
    }
    for (auto it = json.begin(); it != json.end();) {
        if (auto &item = *it; item.is_object()) {
            if (item["type"].get<std::string>() == "dir") {
                if (item.contains("children")) {
                    co_await addPageIDs(ids, item["children"]);
                }
            } else if (item["type"].get<std::string>() == "file") {
                const auto path = item["path"].get<std::string>();
                if (const auto id = ids.find(path + ".mdx"); id != ids.end()) {
                    item["id"] = id->second;
                } else {
                    it = json.erase(it);
                    continue;
                }
            }
        }
        ++it;
    }
}

Task<Json::Value> appendItemSources(const int64_t item) {
    const auto items = co_await global::database->getItemSourceProjects(item);
    Json::Value root(Json::arrayValue);
    for (const auto &res: items) {
        root.append(res);
    }
    co_return root;
}

std::map<int, std::vector<RecipeIngredientItem>> groupIngredients(const std::vector<RecipeIngredientItem> &ingredients, const bool input) {
    std::map<int, std::vector<RecipeIngredientItem>> slots;
    for (auto &ingredient: ingredients) {
        if (ingredient.getValueOfInput() == input) {
            slots[ingredient.getValueOfSlot()].emplace_back(ingredient);
        }
    }
    return slots;
}

namespace service {
    // clang-format off
    ENUM_TO_STR(ProjectError,
        { ProjectError::OK, "ok" },
        { ProjectError::REQUIRES_AUTH, "required_auth" },
        { ProjectError::NO_REPOSITORY, "no_repository" },
        { ProjectError::REPO_TOO_LARGE, "repo_too_large" },
        { ProjectError::NO_BRANCH, "no_branch" },
        { ProjectError::NO_PATH, "no_path" },
        { ProjectError::INVALID_META, "invalid_meta" },
        { ProjectError::INVALID_PAGE, "invalid_page" },
        { ProjectError::DUPLICATE_PAGE, "duplicate_page" },
        { ProjectError::UNKNOWN_RECIPE_TYPE, "unknown_recipe_type" },
        { ProjectError::INVALID_INGREDIENT, "invalid_ingredient" },
        { ProjectError::INVALID_FILE, "invalid_file" },
        { ProjectError::INVALID_FORMAT, "invalid_format" }
    )
    // clang-format on

    std::string formatEditUrl(const Project &project, const std::string &filePath) {
        const auto provider = getGitProvider(project.getValueOfSourceRepo());
        if (!provider) {
            return "";
        }

        fmt::dynamic_format_arg_store<fmt::format_context> store;
        store.push_back(fmt::arg("branch", project.getValueOfSourceBranch()));
        store.push_back(fmt::arg("base", removeLeadingSlash(project.getValueOfSourcePath())));
        store.push_back(fmt::arg("path", removeTrailingSlash(filePath)));
        const auto result = vformat(provider->filePath, store);

        return removeTrailingSlash(project.getValueOfSourceRepo()) + "/" + result;
    }

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

    ResolvedProject::ResolvedProject(const Project &p, const std::filesystem::path &d,
                                     const ProjectVersion &v) :
        project_(p), defaultVersion_(nullptr), docsDir_(d), version_(v),
        projectDb_(std::make_shared<ProjectDatabaseAccess>(*this)) {}

    void ResolvedProject::setDefaultVersion(const ResolvedProject &defaultVersion) {
        defaultVersion_ = std::make_shared<ResolvedProject>(defaultVersion);
    }

    bool ResolvedProject::setLocale(const std::optional<std::string> &locale) {
        if (!locale || hasLocale(*locale)) {
            locale_ = locale.value_or("");
            return true;
        }
        return false;
    }

    bool ResolvedProject::hasLocale(const std::string &locale) const {
        const auto locales = getLocales();
        return locales.contains(locale);
    }

    std::set<std::string> ResolvedProject::getLocales() const {
        std::set<std::string> locales;

        if (const auto localesPath = docsDir_ / I18N_DIR_PATH; exists(localesPath) && is_directory(localesPath)) {
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

    fs::path getFilePath(const fs::path &rootDir, const std::string &path, const std::string &locale) {
        if (!locale.empty()) {
            const auto relativePath = relative(path, rootDir);

            if (const auto localeFile = rootDir / I18N_DIR_PATH / locale / removeLeadingSlash(path); exists(localeFile)) {
                return localeFile;
            }
        }
        return rootDir / removeLeadingSlash(path);
    }

    std::optional<std::string> ResolvedProject::getPagePath(const std::string &path) const {
        const auto filePath = getFilePath(docsDir_, removeLeadingSlash(path) + DOCS_FILE_EXT, locale_);
        if (!exists(filePath))
            return std::nullopt;

        return relative(filePath, docsDir_).string();
    }

    std::tuple<ProjectPage, Error> ResolvedProject::readFile(std::string path) const {
        const auto filePath = getFilePath(docsDir_, removeLeadingSlash(path) + DOCS_FILE_EXT, locale_);

        std::ifstream file(filePath);

        if (!file) {
            return {{"", ""}, Error::ErrInternal};
        }

        std::stringstream buffer;
        buffer << file.rdbuf();

        file.close();

        const auto editUrl = formatEditUrl(project_, path);

        return {{.content = buffer.str(), .editUrl = editUrl}, Error::Ok};
    }

    Task<std::tuple<ProjectPage, Error>> ResolvedProject::readContentPage(const std::string id) const {
        const auto contentPath = co_await projectDb_->getProjectContentPath(id);
        if (!contentPath) {
            co_return {{"", ""}, Error::ErrInternal};
        }
        co_return readFile(*contentPath);
    }

    std::optional<std::string> ResolvedProject::readPageAttribute(std::string path, std::string prop) const {
        const auto filePath = getFilePath(docsDir_, removeLeadingSlash(path), locale_);

        std::ifstream ifs(filePath);

        if (!ifs) {
            return std::nullopt;
        }

        std::string line;
        int frontMatterBorder = 0;
        while (std::getline(ifs, line)) {
            if (frontMatterBorder > 1) {
                break;
            }
            if (line.starts_with("---")) {
                frontMatterBorder++;
            }
            if (frontMatterBorder == 1 && line.starts_with(prop + ":")) {
                const auto pos = line.find(":");
                if (pos != std::string::npos) {
                    auto sub = line.substr(pos + 1);
                    ifs.close();
                    ltrim(sub);
                    rtrim(sub);
                    return sub;
                }
            }
        }
        ifs.close();
        return std::nullopt;
    }

    std::optional<std::string> ResolvedProject::readLangKey(const std::string &locale, const std::string &key) const {
        const auto path = docsDir_ / ".assets" / project_.getValueOfId() / "lang" / (locale + ".json");
        const auto json = parseJsonFile(path);
        if (!json || !json->is_object() || !json->contains(key)) {
            return std::nullopt;
        }
        return (*json)[key].get<std::string>();
    }

    std::tuple<nlohmann::ordered_json, Error> ResolvedProject::getDirectoryTree() const {
        const auto tree = getDirTreeJson(project_, docsDir_, docsDir_, locale_);
        return {tree, Error::Ok};
    }

    std::tuple<nlohmann::ordered_json, Error> ResolvedProject::getContentDirectoryTree() const {
        const auto contentPath = docsDir_ / ".content";
        if (!exists(contentPath)) {
            return {nlohmann::ordered_json(), Error::ErrNotFound};
        }
        const auto tree = getDirTreeJson(project_, docsDir_, contentPath, locale_);
        return {tree, Error::Ok};
    }

    Task<std::tuple<std::optional<nlohmann::ordered_json>, Error>> ResolvedProject::getProjectContents() const {
        std::unordered_map<std::string, std::string> ids;
        for (const auto contents = co_await projectDb_->getProjectItemPages(); const auto &[id, path]: contents) {
            ids.emplace(path, id);
        }

        auto [tree, treeErr] = getContentDirectoryTree();
        if (treeErr != Error::Ok) {
            co_return {std::nullopt, treeErr};
        }

        co_await addPageIDs(ids, tree);

        co_return {tree, Error::Ok};
    }

    std::string getAssetPath(const ResourceLocation &location) {
        return ".assets/" + location.namespace_ + "/" + location.path_ + (location.path_.find('.') != std::string::npos ? "" : ".png");
    }

    std::optional<std::filesystem::path> ResolvedProject::getAsset(const ResourceLocation &location) const {
        if (const auto filePath = docsDir_ / getAssetPath(location); exists(filePath)) {
            return filePath;
        }

        // Legacy asset path fallback
        if (const auto legacyFilePath =
                docsDir_ / getAssetPath({.namespace_ = "item", .path_ = location.namespace_ + '/' + location.path_});
            exists(legacyFilePath))
        {
            return legacyFilePath;
        }

        return std::nullopt;
    }

    const Project &ResolvedProject::getProject() const { return project_; }

    const ProjectVersion &ResolvedProject::getProjectVersion() const { return version_; }

    const std::filesystem::path &ResolvedProject::getDocsDirectoryPath() const { return docsDir_; }

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

                    const GitRevision revision = nlohmann::json::parse(*revisionStr);
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

    int countPagesRecursive(nlohmann::ordered_json json) {
        int sum = 0;

        for (auto &item: json) {
            if (item["type"] == "dir") {
                if (item.contains("children")) {
                    sum += countPagesRecursive(item["children"]);
                }
            } else if (item["type"] == "file") {
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
        const auto path = docsDir_ / DOCS_META_FILE;
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
        const auto [pages, total, size, data] = co_await projectDb_->getProjectItemsDev(params.query, params.page);
        std::vector<FullItemData> itemData;
        for (const auto &[id, path]: data) {
            const auto [name, _] = co_await getItemName(id);
            itemData.emplace_back(id, name, path);
        }
        co_return PaginatedData{.total = total, .pages = pages, .size = size, .data = itemData};
    }

    Task<PaginatedData<FullTagData>> ResolvedProject::getTags(TableQueryParams params) const {
        const auto [pages, total, size, data] = co_await projectDb_->getProjectTagsDev(params.query, params.page);
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
        const auto [pages, total, size, data] = co_await projectDb_->getProjectTagItemsDev(tag, params.query, params.page);
        std::vector<FullItemData> itemData;
        for (const auto &[id, path]: data) {
            const auto [name, _] = co_await getItemName(id);
            itemData.emplace_back(id, name, path);
        }
        co_return PaginatedData{.total = total, .pages = pages, .size = size, .data = itemData};
    }

    Task<PaginatedData<FullRecipeData>> ResolvedProject::getRecipes(const TableQueryParams params) const {
        const auto [pages, total, size, data] = co_await projectDb_->getProjectRecipesDev(params.query, params.page);
        std::vector<FullRecipeData> recipeDate;
        for (const auto &recipe: data) {
            const auto resolved = co_await getRecipe(recipe.getValueOfLoc());
            recipeDate.emplace_back(recipe.getValueOfLoc(), recipe.getValueOfType(), resolved ? parkourJson(*resolved) : nlohmann::json{});
        }
        co_return PaginatedData{.total = total, .pages = pages, .size = size, .data = recipeDate};
    }

    Task<PaginatedData<ProjectVersion>> ResolvedProject::getVersions(const TableQueryParams params) const {
        co_return co_await projectDb_->getVersionsDev(params.query, params.page);
    }

    Task<ItemData> ResolvedProject::getItemName(const Item item) const { co_return co_await getItemName(item.getValueOfLoc()); }

    Task<ItemData> ResolvedProject::getItemName(const std::string loc) const {
        if (const auto path = co_await projectDb_->getProjectContentPath(loc)) {
            const auto title = readPageAttribute(*path, "title");
            co_return ItemData{.name = title.value_or(""), .path = *path};
        }

        const auto projectId = project_.getValueOfId();
        const auto parsed = ResourceLocation::parse(loc);
        const auto localeKey = locale_.empty() ? DEFAULT_LOCALE : locale_;
        const auto localized = readLangKey(localeKey, "item." + projectId + "." + parsed->path_);

        co_return ItemData{.name = localized.value_or(""), .path = ""};
    }

    Task<Json::Value> ResolvedProject::ingredientToJson(const int slot, const std::vector<RecipeIngredientItem> ingredients) const {
        Json::Value root;
        root["slot"] = slot;
        Json::Value itemsJson(Json::arrayValue);
        for (const auto &ingredient: ingredients) {
            const auto item = co_await global::database->getByPrimaryKey<Item>(ingredient.getValueOfItemId());
            const auto [name, path] = co_await getItemName(item);

            Json::Value itemJson;
            itemJson["id"] = item.getValueOfLoc();
            if (!name.empty()) {
                itemJson["name"] = name;
            }
            itemJson["has_page"] = !path.empty();
            itemJson["count"] = ingredient.getValueOfCount();
            itemJson["sources"] = co_await appendItemSources(ingredient.getValueOfItemId());
            itemsJson.append(itemJson);
        }
        root["items"] = itemsJson;
        co_return root;
    }

    Task<Json::Value> ResolvedProject::ingredientToJson(const RecipeIngredientTag &tag) const {
        const auto dbTag = co_await global::database->getByPrimaryKey<Tag>(tag.getValueOfTagId());

        Json::Value root;
        {
            Json::Value tagJson;
            tagJson["id"] = dbTag.getValueOfLoc();
            {
                Json::Value itemsJson(Json::arrayValue);
                for (const auto tagItems = co_await projectDb_->getTagItemsFlat(tag.getValueOfTagId()); const auto &item: tagItems) {
                    const auto [name, path] = co_await getItemName(item);

                    Json::Value itemJson;
                    itemJson["id"] = item.getValueOfLoc();
                    if (!name.empty()) {
                        itemJson["name"] = name;
                    }
                    itemJson["has_page"] = !path.empty();
                    itemJson["sources"] = co_await appendItemSources(item.getValueOfId());
                    itemsJson.append(itemJson);
                }
                tagJson["items"] = itemsJson;
            }
            root["tag"] = tagJson;
        }
        root["count"] = tag.getValueOfCount();
        root["slot"] = tag.getValueOfSlot();
        co_return root;
    }

    Task<std::optional<Json::Value>> ResolvedProject::getRecipe(std::string id) const {
        const auto result = co_await projectDb_->getProjectRecipe(id);
        if (!result) {
            co_return std::nullopt;
        }
        const auto type = result->getValueOfType();
        const auto displaySchema = content::getRecipeType(type);
        if (!displaySchema) {
            co_return std::nullopt;
        }

        const auto itemIngredients =
            co_await global::database->getRelated<RecipeIngredientItem>(RecipeIngredientItem::Cols::_recipe_id, result->getValueOfId());
        const auto tagIngredients =
            co_await global::database->getRelated<RecipeIngredientTag>(RecipeIngredientTag::Cols::_recipe_id, result->getValueOfId());

        Json::Value json;
        json["id"] = result->getValueOfLoc();
        json["type"] = *displaySchema;
        json["type"]["id"] = type;
        {
            Json::Value inputs;
            Json::Value outputs;
            std::map<int, std::vector<RecipeIngredientItem>> itemInputSlots = groupIngredients(itemIngredients, true);
            std::map<int, std::vector<RecipeIngredientItem>> itemOutputSlots = groupIngredients(itemIngredients, false);

            for (const auto &[slot, items]: itemInputSlots) {
                const auto itemJson = co_await ingredientToJson(slot, items);
                inputs.append(itemJson);
            }
            for (const auto &tag: tagIngredients) {
                const auto tagJson = co_await ingredientToJson(tag);
                inputs.append(tagJson);
            }
            for (const auto &[slot, items]: itemOutputSlots) {
                const auto itemJson = co_await ingredientToJson(slot, items);
                outputs.append(itemJson);
            }
            json["inputs"] = inputs;
            json["outputs"] = outputs;
        }

        co_return json;
    }
}
