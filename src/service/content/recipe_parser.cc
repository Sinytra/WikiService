#include "recipe_parser.h"

#include <drogon/drogon.h>
#include "game_content.h"
#include "ingestor_recipes.h"
#include "recipe_builtin.h"
#include "recipe_custom.h"
#include "schemas.h"

using namespace drogon;
using namespace drogon::orm;
using namespace service;
namespace fs = std::filesystem;

namespace content {
    std::vector<std::shared_ptr<RecipeParser>> knownParsers = {std::make_shared<VanillaRecipeParser>(),
                                                               std::make_shared<CustomRecipeParser>()};

    std::shared_ptr<RecipeParser> getRecipeParser(const ResourceLocation &type) {
        for (auto &parser: knownParsers) {
            if (parser->handlesType(type))
                return parser;
        }

        return nullptr;
    }

    std::optional<GameRecipeType> getRecipeType(const ResolvedProject &project, const ResourceLocation &type) {
        const auto parser = getRecipeParser(type);
        return !parser ? std::nullopt : parser->getType(project, type);
    }

    std::optional<PreparedData<StubRecipeType>> RecipesSubIngestor::readRecipeType(const std::string &namespace_,
                                                                                   const std::filesystem::path &root,
                                                                                   const std::filesystem::path &path) const {
        const fs::path relativePath = relative(path, root);
        const auto fileName = relativePath.string();
        // TODO Check resource location validity everywhere
        const auto id = namespace_ + ":" + fileName.substr(0, fileName.find_last_of('.'));
        const ProjectFileIssueCallback issues{issues_, fileName};

        const auto json = parseJsonFile(path);
        if (!json) {
            issues.addIssueAsync(ProjectIssueLevel::ERROR, ProjectIssueType::INGESTOR, ProjectError::INVALID_FILE);
            return std::nullopt;
        }

        if (const auto error = validateJson(schemas::gameRecipeType, *json)) {
            issues.addIssueAsync(ProjectIssueLevel::ERROR, ProjectIssueType::INGESTOR, ProjectError::INVALID_FORMAT, error->format());
            return std::nullopt;
        }

        return PreparedData{.data = StubRecipeType{.id = id}, .issues = issues};
    }

    std::optional<PreparedData<StubRecipe>> RecipesSubIngestor::readRecipe(const std::string &namespace_, const std::filesystem::path &root,
                                                                           const std::filesystem::path &path) const {
        const fs::path relativePath = relative(path, root);
        const auto fileName = relativePath.string();
        const auto id = namespace_ + ":" + fileName.substr(0, fileName.find_last_of('.'));
        const ProjectFileIssueCallback issues{issues_, fileName};

        const auto json = parseJsonFile(path);
        if (!json) {
            issues.addIssueAsync(ProjectIssueLevel::ERROR, ProjectIssueType::INGESTOR, ProjectError::INVALID_FILE);
            return std::nullopt;
        }

        if (const auto error = validateJson(schemas::gameRecipeBase, *json)) {
            issues.addIssueAsync(ProjectIssueLevel::ERROR, ProjectIssueType::INGESTOR, ProjectError::INVALID_FORMAT, error->format());
            return std::nullopt;
        }

        const std::string type = (*json)["type"];
        const auto loc = ResourceLocation::parse(type);
        if (!loc) {
            issues.addIssueAsync(ProjectIssueLevel::ERROR, ProjectIssueType::INGESTOR, ProjectError::INVALID_FORMAT,
                                     "Not a ResourceLocation: " + type);
        }

        const auto parser = getRecipeParser(*loc);
        if (!parser) {
            issues.addIssueAsync(ProjectIssueLevel::ERROR, ProjectIssueType::INGESTOR, ProjectError::UNKNOWN_RECIPE_TYPE, type);
            return std::nullopt;
        }

        if (const auto recipe = parser->parseRecipe(id, type, *json, issues)) {
            return PreparedData{.data = *recipe, .issues = issues};
        }

        return std::nullopt;
    }
}
