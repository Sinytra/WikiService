#pragma once

#include <drogon/drogon.h>
#include <models/Item.h>
#include <service/project/recipe_resolver.h>
#include <service/storage/ingestor/recipe/game_recipes.h>
#include <service/database/database.h>

using namespace drogon_model::postgres;

namespace service {
    // See resolved_db.h
    class ProjectDatabaseAccess;

    enum class FileType { UNKNOWN, DIR, FILE };
    std::string enumToStr(FileType type);
    FileType parseFileType(const std::string &type);
    void to_json(nlohmann::json &j, const FileType &obj);
    void from_json(const nlohmann::json &j, FileType &obj);

    struct FileTreeEntry {
        std::string id;
        std::string name;
        std::string icon;
        std::string path;
        FileType type;
        std::vector<FileTreeEntry> children;

        friend void to_json(nlohmann::json &j, const FileTreeEntry &obj) {
            j = {{"id", obj.id.empty() ? nlohmann::json(nullptr) : nlohmann::json(obj.id)},
                 {"name", obj.name},
                 {"icon", obj.icon.empty() ? nlohmann::json(nullptr) : nlohmann::json(obj.icon)},
                 {"path", obj.path},
                 {"type", obj.type},
                 {"children", obj.children}};
        }

        friend void from_json(const nlohmann::json &j, FileTreeEntry &obj) {
            if (!j["id"].is_null())
                j.at("id").get_to(obj.id);
            j.at("name").get_to(obj.name);
            if (!j["icon"].is_null())
                j.at("icon").get_to(obj.icon);
            j.at("path").get_to(obj.path);
            j.at("type").get_to(obj.type);
            j.at("children").get_to(obj.children);
        }
    };

    typedef std::vector<FileTreeEntry> FileTree;

    struct ProjectPage {
        std::string content;
        std::string editUrl;
    };

    struct Frontmatter {
        std::string id;
        std::string title;
        std::string icon;
    };

    struct ItemContentPage {
        std::string id;
        std::string name;
        std::string icon;
        std::string path;

        friend void to_json(nlohmann::json &j, const ItemContentPage &obj) {
            j = nlohmann::json{{"id", obj.id},
                               {"name", obj.name},
                               {"icon", obj.icon.empty() ? nlohmann::json(nullptr) : nlohmann::json(obj.icon)},
                               {"path", obj.path.empty() ? nlohmann::json(nullptr) : nlohmann::json(obj.path)}};
        }
    };

    struct FullItemData {
        std::string id;
        std::string name;
        std::string path;

        friend void to_json(nlohmann::json &j, const FullItemData &obj) {
            j = nlohmann::json{
                {"id", obj.id}, {"name", obj.name}, {"path", obj.path.empty() ? nlohmann::json(nullptr) : nlohmann::json(obj.path)}};
        }
    };

    struct FullTagData {
        std::string id;
        std::vector<std::string> items;

        friend void to_json(nlohmann::json &j, const FullTagData &obj) {
            j = nlohmann::json{
                {"id", obj.id},
                {"items", obj.items},
            };
        }
    };

    struct FullRecipeData {
        std::string id;
        nlohmann::json data;

        friend void to_json(nlohmann::json &j, const FullRecipeData &obj) { j = nlohmann::json{{"id", obj.id}, {"data", obj.data}}; }
    };

    struct ItemData {
        std::string name;
        std::string path;
    };

    class ProjectBase {
    public:
        virtual ~ProjectBase() = default;

        // Basic info
        virtual std::string getId() const = 0;
        virtual const Project &getProject() const = 0;
        virtual const ProjectVersion &getProjectVersion() const = 0;
        virtual ProjectDatabaseAccess &getProjectDatabase() const = 0;

        // Parameters
        virtual std::string getLocale() const = 0;
        virtual bool hasLocale(const std::string &locale) const = 0;
        virtual std::set<std::string> getLocales() const = 0;

        virtual drogon::Task<std::unordered_map<std::string, std::string>> getAvailableVersions() const = 0;
        virtual drogon::Task<bool> hasVersion(std::string version) const = 0;

        // Pages
        virtual std::optional<std::string> getPagePath(const std::string &path) const = 0; // For project issues
        virtual std::optional<std::string> getPageTitle(const std::string &path) const = 0;
        virtual TaskResult<ProjectPage> readPageFile(std::string path) const = 0;
        virtual drogon::Task<TaskResult<ProjectPage>> readContentPage(std::string id) const = 0;
        virtual std::optional<Frontmatter> readPageAttributes(const std::string &path) const = 0;

        // Game content
        virtual drogon::Task<PaginatedData<ItemContentPage>> getItemContentPages(TableQueryParams params) const = 0;
        virtual drogon::Task<PaginatedData<FullTagData>> getTags(TableQueryParams params) const = 0;
        virtual drogon::Task<PaginatedData<FullItemData>> getTagItems(std::string tag, TableQueryParams params) const = 0;
        virtual drogon::Task<PaginatedData<FullRecipeData>> getRecipes(TableQueryParams params) const = 0;
        virtual drogon::Task<PaginatedData<ProjectVersion>> getVersions(TableQueryParams params) const = 0;

        virtual drogon::Task<TaskResult<ItemData>> getItemName(Item item) const = 0;
        virtual drogon::Task<TaskResult<ItemData>> getItemName(std::string loc) const = 0;

        // Files
        virtual drogon::Task<nlohmann::json> readItemProperties(std::string id) const = 0;
        virtual drogon::Task<std::optional<std::string>> readLangKey(const std::string &namespace_, const std::string &key) const = 0;

        virtual drogon::Task<TaskResult<FileTree>> getDirectoryTree() = 0;
        virtual drogon::Task<TaskResult<FileTree>> getProjectContents() = 0;

        virtual std::optional<std::filesystem::path> getAsset(const ResourceLocation &location) const = 0;
        virtual drogon::Task<std::optional<content::GameRecipeType>> getRecipeType(const ResourceLocation &location) = 0;
        virtual drogon::Task<std::optional<content::ResolvedGameRecipe>> getRecipe(std::string id) = 0;

        // Serialization
        virtual drogon::Task<Json::Value> toJson(bool full = false) const = 0;
        virtual drogon::Task<Json::Value> toJsonVerbose() = 0;
    };

    typedef std::shared_ptr<ProjectBase> ProjectBasePtr;
}
