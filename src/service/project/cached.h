#pragma once

#include <models/Item.h>
#include <service/cache.h>
#include <service/project/project.h>

using namespace std::chrono_literals;

#define DEFAULT_EXPIRE 14 * 24h

template<class T>
concept Optional = requires { typename T::value_type; } && std::same_as<T, std::optional<typename T::value_type>>;

namespace service {
    drogon::Task<> clearProjectCache(std::string projectId);

    class CachedProject final : public ProjectBase, CacheableServiceBase {
    public:
        explicit CachedProject(const ProjectBasePtr &wrapped);

        // Cached methods
        drogon::Task<std::optional<content::ResolvedGameRecipe>> getRecipe(std::string id) override;
        drogon::Task<std::optional<content::GameRecipeType>> getRecipeType(const ResourceLocation &location) override;
        drogon::Task<TaskResult<FileTree>> getDirectoryTree() override;
        drogon::Task<TaskResult<FileTree>> getProjectContents() override;

        // Uncached methods
        std::string getId() const override;
        const Project &getProject() const override;
        const ProjectVersion &getProjectVersion() const override;
        ProjectDatabaseAccess &getProjectDatabase() const override;

        std::string getLocale() const override;
        bool hasLocale(const std::string &locale) const override;
        std::set<std::string> getLocales() const override;
        drogon::Task<std::unordered_map<std::string, std::string>> getAvailableVersions() const override;
        drogon::Task<bool> hasVersion(std::string version) const override;
        std::optional<std::string> getPagePath(const std::string &path) const override;
        std::optional<std::string> getPageTitle(const std::string &path) const override;
        TaskResult<ProjectPage> readPageFile(std::string path) const override;
        drogon::Task<TaskResult<ProjectPage>> readContentPage(std::string id) const override;
        std::optional<Frontmatter> readPageAttributes(const std::string &path) const override;
        drogon::Task<PaginatedData<ItemContentPage>> getItemContentPages(TableQueryParams params) const override;
        drogon::Task<PaginatedData<FullTagData>> getTags(TableQueryParams params) const override;
        drogon::Task<PaginatedData<FullItemData>> getTagItems(std::string tag, TableQueryParams params) const override;
        drogon::Task<PaginatedData<FullRecipeData>> getRecipes(TableQueryParams params) const override;
        drogon::Task<PaginatedData<ProjectVersion>> getVersions(TableQueryParams params) const override;
        drogon::Task<ItemData> getItemName(Item item) const override;
        drogon::Task<ItemData> getItemName(std::string loc) const override;
        drogon::Task<nlohmann::json> readItemProperties(std::string id) const override;
        drogon::Task<std::optional<std::string>> readLangKey(const std::string &namespace_, const std::string &key) const override;
        std::optional<std::filesystem::path> getAsset(const ResourceLocation &location) const override;
        drogon::Task<Json::Value> toJson(bool full) const override;
        drogon::Task<Json::Value> toJsonVerbose() override;

    private:
        template<typename Supplier, typename T = WrapperInnerType_T<std::invoke_result_t<Supplier>>>
        drogon::Task<T> getOrResolveCached(const std::string key, Supplier supplier) {
            // Try get existing cache value
            if (const auto cached = co_await global::cache->getFromCache(key)) {
                if constexpr (Optional<T>) {
                    nlohmann::json json = nlohmann::json::parse(*cached);
                    if (json.is_null()) {
                        co_return T{std::nullopt};
                    }
                    co_return json;
                } else {
                    co_return nlohmann::json::parse(*cached);
                }
            }

            // Check if value is already being computed
            if (const auto pending = co_await getOrStartTask<T>(key)) {
                co_return co_await patientlyAwaitTaskResult(*pending);
            }

            // Compute new value
            const T newValue = co_await supplier();

            // Store in cache
            const nlohmann::json json = newValue;
            co_await global::cache->updateCache(key, json.dump(), DEFAULT_EXPIRE);

            // Complete task
            co_return co_await completeTask<T>(key, newValue);
        }

        std::string createCacheKey(const std::string &base) const;
        std::string createCacheKey(const std::string &base, const std::string &specifier) const;

        ProjectBasePtr wrapped_;
    };
}
