#include "virtual.h"

#include <service/system/lang.h>

using namespace drogon;
using namespace logging;

namespace service {
    Task<std::shared_ptr<VirtualProject>> createVirtualProject(const std::filesystem::path rootDir) {
        const auto proj = co_await global::database->getProjectSource(VIRTUAL_PROJECT_ID);
        if (!proj) {
            logger.error("Default virtual project not found");
            throw std::runtime_error("Error creating virtual project");
        }

        const auto projectVersion = co_await global::database->getDefaultProjectVersion(VIRTUAL_PROJECT_ID);
        if (!projectVersion) {
            logger.error("Default virtual project version not found");
            throw std::runtime_error("Error creating virtual project");
        }

        co_return std::make_shared<VirtualProject>(*proj, *projectVersion, rootDir);
    }

    VirtualProject::VirtualProject(const Project &project, const ProjectVersion &version, const std::filesystem::path &root) :
        project_(project), version_(version), projectDb_(std::make_shared<ProjectDatabaseAccess>(*this)),
        format_(VirtualProjectFormat{root}) {}

    // Basic info
    std::string VirtualProject::getId() const { return VIRTUAL_PROJECT_ID; }
    const Project &VirtualProject::getProject() const { return project_; }
    const ProjectVersion &VirtualProject::getProjectVersion() const { return version_; }
    ProjectDatabaseAccess &VirtualProject::getProjectDatabase() const { return *projectDb_; }

    // Access
    const ProjectFormat &VirtualProject::getFormat() const {
        return format_;
    }

    // Locales TODO
    std::string VirtualProject::getLocale() const { return DEFAULT_LOCALE; }
    bool VirtualProject::hasLocale(const std::string &locale) const { return locale == DEFAULT_LOCALE; }
    std::set<std::string> VirtualProject::getLocales() const { return {DEFAULT_LOCALE}; }

    // Versions
    Task<std::unordered_map<std::string, std::string>> VirtualProject::getAvailableVersions() const { co_return {}; }
    Task<bool> VirtualProject::hasVersion(std::string version) const { co_return false; }

    // Pages
    std::optional<std::string> VirtualProject::getPagePath(const std::string &path) const { return std::nullopt; }
    std::optional<std::string> VirtualProject::getPageTitle(const std::string &path) const { return std::nullopt; }
    TaskResult<ProjectPage> VirtualProject::readPageFile(std::string path) const { return Error::ErrNotFound; }
    Task<TaskResult<ProjectPage>> VirtualProject::readContentPage(std::string id) const { co_return Error::ErrNotFound; }
    std::optional<Frontmatter> VirtualProject::readPageAttributes(const std::string &path) const { return std::nullopt; }

    // Dev tables
    Task<PaginatedData<ItemContentPage>> VirtualProject::getItemContentPages(TableQueryParams params) const {
        co_return {.total = 0, .pages = 0, .size = 0};
    }
    Task<PaginatedData<FullTagData>> VirtualProject::getTags(TableQueryParams params) const {
        co_return {.total = 0, .pages = 0, .size = 0};
    }
    Task<PaginatedData<FullItemData>> VirtualProject::getTagItems(std::string tag, TableQueryParams params) const {
        co_return {.total = 0, .pages = 0, .size = 0};
    }
    Task<PaginatedData<FullRecipeData>> VirtualProject::getRecipes(TableQueryParams params) const {
        co_return {.total = 0, .pages = 0, .size = 0};
    }
    Task<PaginatedData<ProjectVersion>> VirtualProject::getVersions(TableQueryParams params) const {
        co_return {.total = 0, .pages = 0, .size = 0};
    }

    Task<TaskResult<ItemData>> VirtualProject::getItemName(const Item item) const { co_return co_await getItemName(item.getValueOfLoc()); }
    Task<TaskResult<ItemData>> VirtualProject::getItemName(const std::string loc) const {
        const auto name = co_await global::lang->getItemName(DEFAULT_LOCALE, loc);
        if (!name) {
            co_return Error::ErrNotFound;
        }
        co_return ItemData{*name, ""};
    }
    Task<nlohmann::json> VirtualProject::readItemProperties(std::string id) const { co_return {}; }

    Task<TaskResult<FileTree>> VirtualProject::getDirectoryTree() { co_return Error::ErrNotFound; }
    Task<TaskResult<FileTree>> VirtualProject::getProjectContents() { co_return Error::ErrNotFound; }

    Task<std::optional<std::string>> VirtualProject::readLangKey(const std::string &namespace_, const std::string &key) const {
        const ResourceLocation location{namespace_, key};
        co_return co_await global::lang->getItemName(std::nullopt, location);
    }

    std::optional<std::filesystem::path> VirtualProject::getAsset(const ResourceLocation &location) const {
        return std::nullopt; // TODO
    }

    Task<std::optional<content::GameRecipeType>> VirtualProject::getRecipeType(const ResourceLocation &location) { co_return std::nullopt; }
    Task<std::optional<content::ResolvedGameRecipe>> VirtualProject::getRecipe(std::string id) { co_return std::nullopt; }

    Task<Json::Value> VirtualProject::toJson(const bool full) const { throw std::runtime_error("toJson not supported in virtual project"); }
    Task<Json::Value> VirtualProject::toJsonVerbose() { throw std::runtime_error("toJsonVerbose not supported in virtual project"); }
}
