#include <schemas/schemas.h>
#include <service/database/project_database.h>
#include <service/project/recipe_resolver.h>
#include <service/project/resolved.h>

using namespace logging;
using namespace drogon;
using namespace drogon_model::postgres;
namespace fs = std::filesystem;

namespace service {
    nlohmann::json parseItemProperties(const std::filesystem::path &path, const std::string &id) {
        const auto json = parseJsonFile(path);
        if (!json) {
            return nullptr;
        }

        if (const auto error = validateJson(schemas::properties, *json)) {
            return nullptr;
        }

        if (!json->contains(id)) {
            return nullptr;
        }

        return (*json)[id];
    }

    Task<nlohmann::json> ResolvedProject::readItemProperties(const std::string id) const {
        const auto filePath = format_.getItemPropertiesPath();
        co_return parseItemProperties(filePath, id);
    }

    void ResolvedProject::addPageMetadata(FileTree &tree) const {
        for (auto it = tree.begin(); it != tree.end();) {
            auto entry = it;

            if (entry->type == FileType::DIR) {
                addPageMetadata(entry->children);
            } else if (entry->type == FileType::FILE) {
                if (const auto page = readPageAttributes(entry->path + DOCS_FILE_EXT)) {
                    entry->id = page->id;
                    entry->icon = page->icon;
                } else {
                    it = tree.erase(it);
                    continue;
                }
            }
            ++it;
        }
    }

    Task<TaskResult<FileTree>> ResolvedProject::getProjectContents() {
        const auto contentPath = getFormat().getContentDirectoryPath();
        if (!exists(contentPath)) {
            co_return Error::ErrNotFound;
        }
        auto tree = getDirectoryTree(contentPath);
        addPageMetadata(tree);
        co_return tree;
    }

    Task<std::optional<std::string>> ResolvedProject::readLangKey(const std::string &namespace_, const std::string &key) const {
        const auto path = format_.getLanguageFilePath(namespace_);
        const auto json = parseJsonFile(path);
        if (!json || !json->is_object() || !json->contains(key)) {
            co_return std::nullopt;
        }
        co_return (*json)[key].get<std::string>();
    }

    Task<TaskResult<ItemData>> ResolvedProject::getItemName(const Item item) const { co_return co_await getItemName(item.getValueOfLoc()); }

    Task<TaskResult<ItemData>> ResolvedProject::getItemName(const std::string loc) const {
        const auto projectId = project_.getValueOfId();
        const auto parsed = ResourceLocation::parse(loc);

        auto localized = co_await readLangKey(parsed->namespace_, "item." + parsed->namespace_ + "." + parsed->path_);
        if (!localized) {
            localized = co_await readLangKey(parsed->namespace_, "block." + parsed->namespace_ + "." + parsed->path_);
        }

        const auto path = co_await projectDb_->getProjectContentPath(loc);
        if (!localized) {
            // Use page title instead
            if (const auto title = getPageTitle(*path)) {
                co_return ItemData{.name = *title, .path = *path};
            }
            co_return Error::ErrNotFound;
        }

        co_return ItemData{.name = *localized, .path = path.value_or("")};
    }

    Task<std::optional<content::GameRecipeType>> ResolvedProject::getRecipeType(const ResourceLocation &location) {
        const auto dataRoot = format_.getDataRoot();
        const auto path = dataRoot / location.namespace_ / "recipe_type" / (location.path_ + ".json");
        const auto json = parseJsonFile(path);
        if (!json) {
            co_return std::nullopt;
        }
        if (const auto error = validateJson(schemas::gameRecipeType, *json)) {
            logger.error("Invalid recipe type {} in project {}: {}", std::string(location), project_.getValueOfId(), error->format());
            co_return std::nullopt;
        }
        co_return *json;
    }

    Task<std::optional<content::ResolvedGameRecipe>> ResolvedProject::getRecipe(const std::string id) {
        const auto recipe = co_await projectDb_->getProjectRecipe(id);
        if (!recipe) {
            co_return std::nullopt;
        }
        co_return co_await content::resolveRecipe(*recipe, getLocale());
    }
}
