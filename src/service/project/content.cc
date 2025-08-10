#include <service/content/recipe_resolver.h>
#include <service/database/resolved_db.h>
#include <service/project/properties.h>
#include <service/resolved.h>
#include <service/schemas.h>

using namespace logging;
using namespace drogon;
using namespace drogon_model::postgres;
namespace fs = std::filesystem;

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
                if (const auto id = ids.find(path + DOCS_FILE_EXT); id != ids.end()) {
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

namespace service {
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

    Task<nlohmann::json> ResolvedProject::readItemProperties(const std::string id) const {
        const auto filePath = format_.getItemPropertiesPath();
        co_return content::parseItemProperties(filePath, id);
    }

    std::optional<std::string> ResolvedProject::readLangKey(const std::string &key) const {
        const auto path = format_.getLanguageFilePath(project_.getValueOfModid());
        const auto json = parseJsonFile(path);
        if (!json || !json->is_object() || !json->contains(key)) {
            return std::nullopt;
        }
        return (*json)[key].get<std::string>();
    }

    Task<ItemData> ResolvedProject::getItemName(const Item item) const { co_return co_await getItemName(item.getValueOfLoc()); }

    Task<ItemData> ResolvedProject::getItemName(const std::string loc) const {
        const auto projectId = project_.getValueOfId();
        const auto parsed = ResourceLocation::parse(loc);

        auto localized = readLangKey("item." + projectId + "." + parsed->path_);
        if (!localized) {
            localized = readLangKey("block." + projectId + "." + parsed->path_);
        }

        // Use page title instead
        if (!localized) {
            if (const auto path = co_await projectDb_->getProjectContentPath(loc)) {
                const auto title = getPageTitle(*path);
                co_return ItemData{.name = title.value_or(""), .path = *path};
            }
        }

        co_return ItemData{.name = localized.value_or(""), .path = ""};
    }

    // TODO Cache
    std::optional<content::GameRecipeType> ResolvedProject::getRecipeType(const ResourceLocation &location) const {
        const auto path = docsDir_ / ".data" / location.namespace_ / "recipe_type" / (location.path_ + ".json");
        const auto json = parseJsonFile(path);
        if (!json) {
            return std::nullopt;
        }
        if (const auto error = validateJson(schemas::gameRecipeType, *json)) {
            logger.error("Invalid recipe type {} in project {}: {}", std::string(location), project_.getValueOfId(), error->format());
            return std::nullopt;
        }
        return std::make_optional<content::GameRecipeType>(*json);
    }

    Task<std::optional<content::ResolvedGameRecipe>> ResolvedProject::getRecipe(const std::string id) const {
        const auto recipe = co_await projectDb_->getProjectRecipe(id);
        if (!recipe) {
            co_return std::nullopt;
        }
        co_return co_await content::resolveRecipe(*recipe, getLocale());
    }
}