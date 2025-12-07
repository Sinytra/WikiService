#include "cached.h"

using namespace drogon;

namespace service {
    Task<> clearProjectCache(const std::string projectId) {
        const auto prefix = std::format("pcache:{}", projectId);
        co_await global::cache->eraseAll(prefix);
    }

    CachedProject::CachedProject(const ProjectBasePtr &wrapped) : wrapped_(wrapped) {}

    std::string CachedProject::createCacheKey(const std::string &base) const { return std::format("pcache:{}:{}", getId(), base); }

    std::string CachedProject::createCacheKey(const std::string &base, const std::string &specifier) const {
        return std::format("pcache:{}:{}:{}", getId(), base, specifier);
    }

    Task<std::optional<content::ResolvedGameRecipe>> CachedProject::getRecipe(const std::string id) {
        co_return co_await getOrResolveCached(createCacheKey("recipe", id), std::bind_front(&ProjectBase::getRecipe, wrapped_, id));
    }

    Task<std::optional<content::GameRecipeType>> CachedProject::getRecipeType(const ResourceLocation &location) {
        co_return co_await getOrResolveCached(createCacheKey("recipe_type", location),
                                              std::bind_front(&ProjectBase::getRecipeType, wrapped_, location));
    }

    Task<TaskResult<FileTree>> CachedProject::getDirectoryTree() {
        co_return co_await getOrResolveCached(createCacheKey("directory_tree"), std::bind_front(&ProjectBase::getDirectoryTree, wrapped_));
    }

    Task<TaskResult<FileTree>> CachedProject::getProjectContents() {
        co_return co_await getOrResolveCached(createCacheKey("content_tree"), std::bind_front(&ProjectBase::getProjectContents, wrapped_));
    }

    // Uncached methods
    std::string CachedProject::getId() const { return wrapped_->getId(); }
    const Project &CachedProject::getProject() const { return wrapped_->getProject(); }
    const ProjectVersion &CachedProject::getProjectVersion() const { return wrapped_->getProjectVersion(); }
    ProjectDatabaseAccess &CachedProject::getProjectDatabase() const { return wrapped_->getProjectDatabase(); }

    Task<Json::Value> CachedProject::toJson(bool full) const { co_return co_await wrapped_->toJson(); }
    Task<Json::Value> CachedProject::toJsonVerbose() { co_return co_await wrapped_->toJsonVerbose(); }
    std::string CachedProject::getLocale() const { return wrapped_->getLocale(); }
    bool CachedProject::hasLocale(const std::string &locale) const { return wrapped_->hasLocale(locale); }
    std::set<std::string> CachedProject::getLocales() const { return wrapped_->getLocales(); }
    Task<std::unordered_map<std::string, std::string>> CachedProject::getAvailableVersions() const {
        co_return co_await wrapped_->getAvailableVersions();
    }
    Task<bool> CachedProject::hasVersion(const std::string version) const { co_return co_await wrapped_->hasVersion(version); }
    std::optional<std::string> CachedProject::getPagePath(const std::string &path) const { return wrapped_->getPagePath(path); }
    std::optional<std::string> CachedProject::getPageTitle(const std::string &path) const { return wrapped_->getPageTitle(path); }
    TaskResult<ProjectPage> CachedProject::readPageFile(const std::string path) const { return wrapped_->readPageFile(path); }
    Task<TaskResult<ProjectPage>> CachedProject::readContentPage(const std::string id) const {
        co_return co_await wrapped_->readContentPage(id);
    }
    std::optional<Frontmatter> CachedProject::readPageAttributes(const std::string &path) const {
        return wrapped_->readPageAttributes(path);
    }
    Task<PaginatedData<ItemContentPage>> CachedProject::getItemContentPages(const TableQueryParams params) const {
        co_return co_await wrapped_->getItemContentPages(params);
    }
    Task<PaginatedData<FullTagData>> CachedProject::getTags(const TableQueryParams params) const {
        co_return co_await wrapped_->getTags(params);
    }
    Task<PaginatedData<FullItemData>> CachedProject::getTagItems(const std::string tag, const TableQueryParams params) const {
        co_return co_await wrapped_->getTagItems(tag, params);
    }
    Task<PaginatedData<FullRecipeData>> CachedProject::getRecipes(const TableQueryParams params) const {
        co_return co_await wrapped_->getRecipes(params);
    }
    Task<PaginatedData<ProjectVersion>> CachedProject::getVersions(const TableQueryParams params) const {
        co_return co_await wrapped_->getVersions(params);
    }
    Task<TaskResult<ItemData>> CachedProject::getItemName(const Item item) const { co_return co_await wrapped_->getItemName(item); }
    Task<TaskResult<ItemData>> CachedProject::getItemName(const std::string loc) const { co_return co_await wrapped_->getItemName(loc); }
    Task<nlohmann::json> CachedProject::readItemProperties(const std::string id) const {
        co_return co_await wrapped_->readItemProperties(id);
    }
    Task<std::optional<std::string>> CachedProject::readLangKey(const std::string &namespace_, const std::string &key) const {
        co_return co_await wrapped_->readLangKey(namespace_, key);
    }
    std::optional<std::filesystem::path> CachedProject::getAsset(const ResourceLocation &location) const {
        return wrapped_->getAsset(location);
    }
    const ProjectFormat &CachedProject::getFormat() const {
        return wrapped_->getFormat();
    }
}
