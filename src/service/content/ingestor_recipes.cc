#include "ingestor_recipes.h"
#include <database/database.h>
#include <database/resolved_db.h>
#include <drogon/drogon.h>
#include "game_content.h"

using namespace drogon;
using namespace drogon::orm;
using namespace service;
namespace fs = std::filesystem;

namespace content {
    RecipesSubIngestor::RecipesSubIngestor(const ResolvedProject &proj, const std::shared_ptr<spdlog::logger> &log,
                                           ProjectIssueCallback &issues) : SubIngestor(proj, log, issues) {}

    Task<PreparationResult> RecipesSubIngestor::prepare() {
        PreparationResult result;

        const auto docsRoot = project_.getDocsDirectoryPath();
        const auto modid = project_.getProject().getValueOfModid();
        const auto recipesRoot = docsRoot / ".data" / modid / "recipe";

        if (!exists(recipesRoot)) {
            co_return result;
        }

        for (const auto &entry: fs::recursive_directory_iterator(recipesRoot)) {
            if (!entry.is_regular_file() || entry.path().extension() != EXT_JSON) {
                continue;
            }

            if (const auto recipe = readRecipe(modid, recipesRoot, entry.path())) {
                recipes_.push_back(*recipe);

                for (const auto &item: recipe->ingredients) {
                    result.items.insert(item.itemId);
                }
            } else {
                logger_->warn("Skipping recipe file: '{}'", entry.path().filename().string());
            }
        }

        co_return result;
    }

    Task<Error> RecipesSubIngestor::addRecipe(const ModRecipe recipe) const {
        const auto clientPtr = project_.getProjectDatabase().getDbClientPtr();
        CoroMapper<Recipe> recipeMapper(clientPtr);

        const auto [id, type, ingredients] = recipe;
        const auto versionId = project_.getProjectVersion().getValueOfId();

        // language=postgresql
        const auto row =
            co_await clientPtr->execSqlCoro("INSERT INTO recipe VALUES (DEFAULT, $1, $2, $3) RETURNING id", versionId, id, type);
        const auto recipeId = row.front().at("id").as<int64_t>();

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
            }
            co_await recipeMapper.deleteByPrimaryKey(recipeId);
            co_return Error::ErrNotFound;
        }

        co_return Error::Ok;
    }

    Task<Error> RecipesSubIngestor::execute() {
        logger_->info("Adding {} recipes", recipes_.size());

        for (const auto &recipe: recipes_) {
            logger_->trace("Registering recipe '{}'", recipe.id);
            co_await addRecipe(recipe);
        }

        co_return Error::Ok;
    }

}
