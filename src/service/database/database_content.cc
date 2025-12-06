#include "database.h"

using namespace logging;
using namespace drogon;
using namespace drogon::orm;

namespace service {
    Task<TaskResult<>> Database::refreshFlatTagItemView() const {
        co_return co_await handleDatabaseOperation([](const DbClientPtr &client) -> Task<> {
            // language=postgresql
            co_await client->execSqlCoro("REFRESH MATERIALIZED VIEW tag_item_flat;");
        });
    }

    Task<int64_t> Database::addRecipe(const std::string id, const std::string type) const {
        // language=postgresql
        static constexpr auto query = "INSERT INTO recipe VALUES (DEFAULT, NULL, $1, $2) ON CONFLICT DO NOTHING RETURNING id";
        const auto res = co_await handleDatabaseOperation([&, id, type](const DbClientPtr &client) -> Task<int64_t> {
            const auto result = co_await client->execSqlCoro(query, id, type);
            co_return result.size() < 1 ? 0 : result.front().at("id").as<int64_t>();
        });
        co_return res.value_or(-1);
    }

    Task<TaskResult<>> Database::addRecipeIngredientItem(const int64_t recipe_id, const std::string item, const int slot, const int count,
                                                         const bool input) const {
        // language=postgresql
        static constexpr auto query = "INSERT INTO recipe_ingredient_item (recipe_id, item_id, slot, count, input) \
                                       SELECT $1 as recipe_id, id, $3 as slot, $4 as count, $5 as input \
                                       FROM item \
                                       WHERE item.loc = $2";

        co_return co_await handleDatabaseOperation([&, recipe_id, item, slot, count, input](const DbClientPtr &client) -> Task<> {
            co_await client->execSqlCoro(query, recipe_id, item, slot, count, input);
        });
    }

    Task<TaskResult<>> Database::addRecipeIngredientTag(const int64_t recipe_id, const std::string tag, const int slot, const int count,
                                                        const bool input) const {
        // language=postgresql
        static constexpr auto query = "INSERT INTO recipe_ingredient_tag (recipe_id, tag_id, slot, count, input) \
                                       SELECT $1 as recipe_id, id, $3 as slot, $4 as count, $5 as input \
                                       FROM tag \
                                       WHERE tag.loc = $2";
        co_return co_await handleDatabaseOperation([&, recipe_id, tag, slot, count, input](const DbClientPtr &client) -> Task<> {
            co_await client->execSqlCoro(query, recipe_id, tag, slot, count, input);
        });
    }

    // TODO Bind to project
    Task<std::vector<GlobalItem>> Database::getGlobalTagItems(int64_t tagId) const {
        // language=postgresql
        static constexpr auto query = "SELECT ver.id, ver.name, ver.project_id, item.loc \
                                       FROM tag_item_flat \
                                       JOIN project_item pitem ON pitem.id = child \
                                       JOIN item ON item.id = pitem.item_id \
                                       LEFT JOIN project_version ver ON ver.id = pitem.version_id \
                                       WHERE parent IN (SELECT ptag.id FROM project_tag ptag \
                                           JOIN tag t on ptag.tag_id = t.id \
                                           WHERE t.id = $1)";
        const auto res = co_await handleDatabaseOperation([&, tagId](const DbClientPtr &client) -> Task<std::vector<GlobalItem>> {
            const auto results = co_await client->execSqlCoro(query, tagId);
            std::vector<GlobalItem> tagItems;
            for (const auto &row: results) {
                const auto versionId = row[0].isNull() ? 0 : row[0].as<int64_t>();
                const auto versionName = row[1].isNull() ? "" : row[1].as<std::string>();
                const auto projectId = row[2].isNull() ? "" : row[2].as<std::string>();
                const auto itemLocation = row[3].as<std::string>();
                tagItems.emplace_back(versionId, versionName, projectId, itemLocation);
            }
            co_return tagItems;
        });
        co_return res.value_or({});
    }
}
