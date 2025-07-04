#include "ingestor_recipes.h"
#include <database/database.h>
#include <database/resolved_db.h>
#include <drogon/drogon.h>
#include "ingestor.h"

using namespace drogon;
using namespace drogon::orm;
using namespace service;
namespace fs = std::filesystem;

namespace content {
    RecipesSubIngestor::RecipesSubIngestor(const ResolvedProject &proj, const std::shared_ptr<spdlog::logger> &log,
                                           ProjectFileIssueCallback &issues) : SubIngestor(proj, log, issues) {}

    Task<PreparationResult> RecipesSubIngestor::prepare() {
        PreparationResult result;

        const auto docsRoot = project_.getDocsDirectoryPath();
        const auto modid = project_.getProject().getValueOfModid();

        if (const auto recipesRoot = docsRoot / ".data" / modid / "recipe"; exists(recipesRoot)) {
            for (const auto &entry: fs::recursive_directory_iterator(recipesRoot)) {
                if (!entry.is_regular_file() || entry.path().extension() != EXT_JSON) {
                    continue;
                }

                if (const auto recipe = readRecipe(modid, recipesRoot, entry.path())) {
                    recipes_.push_back(*recipe);

                    for (const auto &item: recipe->data.ingredients) {
                        result.items.insert(item.itemId);
                    }
                } else {
                    logger_->warn("Skipping recipe file: '{}'", entry.path().filename().string());
                }
            }
        }

        if (const auto recipeTypesRoot = docsRoot / ".data" / modid / "recipe_type"; exists(recipeTypesRoot)) {
            for (const auto &entry: fs::recursive_directory_iterator(recipeTypesRoot)) {
                if (!entry.is_regular_file() || entry.path().extension() != EXT_JSON) {
                    continue;
                }

                if (const auto recipe = readRecipeType(modid, recipeTypesRoot, entry.path())) {
                    recipeTypes_.push_back(*recipe);
                } else {
                    logger_->warn("Skipping recipe type file: '{}'", entry.path().filename().string());
                }
            }
        }

        co_return result;
    }

    Task<Error> RecipesSubIngestor::addRecipeType(const PreparedData<StubRecipeType> type) const {
        const auto clientPtr = project_.getProjectDatabase().getDbClientPtr();
        const auto versionId = project_.getProjectVersion().getValueOfId();
        CoroMapper<RecipeType> recipeTypeMapper(clientPtr);

        RecipeType dbType;
        dbType.setVersionId(versionId);
        dbType.setLoc(type.data.id);
        co_await recipeTypeMapper.insert(dbType);

        co_return Error::Ok;
    }

    Task<Error> RecipesSubIngestor::addRecipe(const PreparedData<StubRecipe> recipe) const {
        const auto clientPtr = project_.getProjectDatabase().getDbClientPtr();
        CoroMapper<Recipe> recipeMapper(clientPtr);
        CoroMapper<RecipeType> recipeTypeMapper(clientPtr);

        const auto [id, type, ingredients] = recipe.data;
        const auto versionId = project_.getProjectVersion().getValueOfId();

        const auto recipeType = co_await project_.getProjectDatabase().getRecipeType(type);
        if (!recipeType) {
            recipe.issues.addIssueAsync(ProjectIssueLevel::ERROR, ProjectIssueType::INGESTOR, ProjectError::UNKNOWN_RECIPE_TYPE, type);
            co_return Error::ErrNotFound;
        }

        Recipe dbRecipe;
        dbRecipe.setVersionId(versionId);
        dbRecipe.setLoc(id);
        dbRecipe.setTypeId(recipeType->getValueOfId());
        const auto insertedRecipe = co_await recipeMapper.insert(dbRecipe);
        const auto recipeId = insertedRecipe.getValueOfId();

        for (auto &[ingredientId, slot, count, input, isTag]: ingredients) {
            try {
                if (isTag) {
                    // language=postgresql
                    co_await clientPtr->execSqlCoro("INSERT INTO recipe_ingredient_tag (recipe_id, tag_id, slot, count, input) \
                                                        SELECT $1 as recipe_id, id, $3 as slot, $4 as count, $5 as input \
                                                        FROM tag \
                                                        WHERE tag.loc = $2;",
                                                    recipeId, ingredientId, slot, count, input);
                } else {
                    // language=postgresql
                    co_await clientPtr->execSqlCoro("INSERT INTO recipe_ingredient_item (recipe_id, item_id, slot, count, input) \
                                                        SELECT $1 as recipe_id, id, $3 as slot, $4 as count, $5 as input \
                                                        FROM item \
                                                        WHERE item.loc = $2;",
                                                    recipeId, ingredientId, slot, count, input);
                }
                continue;
            } catch ([[maybe_unused]] const std::exception &e) {
                const auto ingredientName = isTag ? "#" + ingredientId : ingredientId;
                logger_->warn("Skipping recipe '{}' due to missing ingredient '{}'", id, ingredientName);
                recipe.issues.addIssueAsync(ProjectIssueLevel::ERROR, ProjectIssueType::INGESTOR, ProjectError::INVALID_INGREDIENT, ingredientName);
            }
            co_await recipeMapper.deleteByPrimaryKey(recipeId);
            co_return Error::ErrNotFound;
        }

        co_return Error::Ok;
    }

    Task<Error> RecipesSubIngestor::execute() {
        logger_->info("Adding {} recipes types", recipeTypes_.size());

        for (const auto &type: recipeTypes_) {
            logger_->trace("Registering recipe type '{}'", type.data.id);
            co_await addRecipeType(type);
        }

        logger_->info("Adding {} recipes", recipes_.size());

        for (const auto &recipe: recipes_) {
            logger_->trace("Registering recipe '{}'", recipe.data.id);
            co_await addRecipe(recipe);
        }

        co_return Error::Ok;
    }

}
