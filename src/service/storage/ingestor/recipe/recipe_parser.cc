#include "recipe_parser.h"
#include "recipe_builtin.h"
#include "recipe_custom.h"

#include <schemas/schemas.h>
#include <service/storage/ingestor/ingestor.h>

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

    Task<std::optional<GameRecipeType>> getRecipeType(const ProjectBasePtr project, const ResourceLocation &type) {
        const auto parser = getRecipeParser(type);
        co_return !parser ? std::nullopt : co_await parser->getType(project, type);
    }

    std::optional<PreparedData<StubRecipeType>> RecipesSubIngestor::readRecipeType(const std::string &namespace_,
                                                                                   const std::filesystem::path &root,
                                                                                   const std::filesystem::path &path) const {
        const ProjectFileIssueCallback fileIssues{issues_, path};

        const fs::path relativePath = relative(path, root);
        const auto fileName = relativePath.string();
        const auto id = namespace_ + ":" + fileName.substr(0, fileName.find_last_of('.'));

        if (!fileIssues.validateResourceLocation(id)) {
            return std::nullopt;
        }

        if (const auto json = fileIssues.readAndValidateJson(schemas::gameRecipeType); !json) {
            return std::nullopt;
        }

        return PreparedData{.data = StubRecipeType{.id = id}, .issues = fileIssues};
    }

    std::optional<PreparedData<StubRecipe>> RecipesSubIngestor::readRecipe(const std::string &namespace_, const std::filesystem::path &root,
                                                                           const std::filesystem::path &path) const {
        const ProjectFileIssueCallback fileIssues{issues_, path};

        // TODO Shorthand
        const fs::path relativePath = relative(path, root);
        const auto fileName = relativePath.string();
        const auto id = namespace_ + ":" + fileName.substr(0, fileName.find_last_of('.'));

        if (!fileIssues.validateResourceLocation(id)) {
            return std::nullopt;
        }

        const auto json = fileIssues.readAndValidateJson(schemas::gameRecipeBase);
        if (!json) {
            return std::nullopt;
        }

        const std::string type = (*json)["type"];
        const auto loc = fileIssues.validateResourceLocation(type);
        if (!loc) {
            return std::nullopt;
        }

        const auto parser = getRecipeParser(*loc);
        if (!parser) {
            fileIssues.addIssueAsync(ProjectIssueLevel::ERROR, ProjectIssueType::INGESTOR, ProjectError::UNKNOWN_RECIPE_TYPE, type);
            return std::nullopt;
        }

        try {
            if (const auto recipe = parser->parseRecipe(id, type, *json, fileIssues)) {
                return PreparedData{.data = *recipe, .issues = fileIssues};
            }
        } catch (std::exception &err) {
            logger_->error("Error parsing recipe {} ({}): {}", id, type, err.what());
            throw;
        }

        return std::nullopt;
    }
}
