#include "database.h"

using namespace logging;
using namespace drogon;
using namespace drogon::orm;

namespace service {
    Task<Error> Database::refreshFlatTagItemView() const {
        const auto [res, err] = co_await handleDatabaseOperation<Error>([](const DbClientPtr &client) -> Task<Error> {
            // language=postgresql
            co_await client->execSqlCoro("REFRESH MATERIALIZED VIEW tag_item_flat;");
            co_return Error::Ok;
        });
        co_return res.value_or(err);
    }

    Task<Error> Database::addItem(const std::string item) const {
        // language=postgresql
        static constexpr auto query = "INSERT INTO project_item (item_id, version_id) \
                                       SELECT id, NULL FROM item WHERE loc = $1 \
                                       ON CONFLICT DO NOTHING";

        const auto [res, err] = co_await handleDatabaseOperation<Error>([item](const DbClientPtr &client) -> Task<Error> {
            // language=postgresql
            co_await client->execSqlCoro("INSERT INTO item VALUES (DEFAULT, $1) ON CONFLICT DO NOTHING", item);
            co_await client->execSqlCoro(query, item);
            co_return Error::Ok;
        });
        co_return res.value_or(err);
    }

    Task<Error> Database::addTag(const std::string tag) const {
        // language=postgresql
        static constexpr auto query = "INSERT INTO project_tag (tag_id, version_id) \
                                       SELECT id, NULL FROM tag WHERE loc = $1 \
                                       ON CONFLICT DO NOTHING";

        const auto [res, err] = co_await handleDatabaseOperation<Error>([tag](const DbClientPtr &client) -> Task<Error> {
            // language=postgresql
            co_await client->execSqlCoro("INSERT INTO tag VALUES (DEFAULT, $1) ON CONFLICT DO NOTHING", tag);
            co_await client->execSqlCoro(query, tag);
            co_return Error::Ok;
        });
        co_return res.value_or(err);
    }

    Task<Error> Database::addTagItemEntry(const std::string tag, const std::string item) const {
        // language=postgresql
        static constexpr auto tagItemQuery = "INSERT INTO tag_item (tag_id, item_id) \
                                              SELECT pt.id, pip.id FROM project_tag pt CROSS JOIN project_item pip \
                                              JOIN tag ON tag.id = pt.tag_id \
                                              JOIN item ON item.id = pip.item_id \
                                              WHERE pt.version_id IS NULL \
                                                AND pip.version_id IS NULL \
                                                AND tag.loc = $1 AND item.loc = $2 \
                                              ON CONFLICT DO NOTHING";
        const auto [res, err] = co_await handleDatabaseOperation<Error>([&, tag, item](const DbClientPtr &client) -> Task<Error> {
            co_await client->execSqlCoro(tagItemQuery, tag, item);
            co_return Error::Ok;
        });
        co_return res.value_or(Error::ErrInternal);
    }

    Task<Error> Database::addTagTagEntry(const std::string parentTag, const std::string childTag) const {
        // language=postgresql
        static constexpr auto tagTagQuery = "INSERT INTO tag_tag (parent, child) \
                                             SELECT tp.id, tc.id FROM project_tag tp CROSS JOIN project_tag tc \
                                             JOIN tag p ON p.id = tp.tag_id \
                                             JOIN tag c ON c.id = tc.tag_id \
                                             WHERE tp.version_id IS NULL \
                                               AND tc.version_id IS NULL \
                                               AND p.loc = $1 AND c.loc = $2 \
                                             ON CONFLICT DO NOTHING";
        const auto [res, err] = co_await handleDatabaseOperation<Error>([&, parentTag, childTag](const DbClientPtr &client) -> Task<Error> {
            co_await client->execSqlCoro(tagTagQuery, parentTag, childTag);
            co_return Error::Ok;
        });
        co_return res.value_or(err);
    }

    Task<int64_t> Database::addRecipe(const std::string id, const std::string type) const {
        // language=postgresql
        static constexpr auto query = "INSERT INTO recipe VALUES (DEFAULT, NULL, $1, $2) ON CONFLICT DO NOTHING RETURNING id";
        const auto [res, err] = co_await handleDatabaseOperation<int64_t>([&, id, type](const DbClientPtr &client) -> Task<int64_t> {
            const auto result = co_await client->execSqlCoro(query, id, type);
            co_return result.size() < 1 ? 0 : result.front().at("id").as<int64_t>();
        });
        co_return res.value_or(-1);
    }

    Task<Error> Database::addRecipeIngredientItem(const int64_t recipe_id, const std::string item, const int slot, const int count,
                                                  const bool input) const {
        // language=postgresql
        static constexpr auto query = "INSERT INTO recipe_ingredient_item (recipe_id, item_id, slot, count, input) \
                                       SELECT $1 as recipe_id, id, $3 as slot, $4 as count, $5 as input \
                                       FROM item \
                                       WHERE item.loc = $2";
        const auto [res, err] =
            co_await handleDatabaseOperation<Error>([&, recipe_id, item, slot, count, input](const DbClientPtr &client) -> Task<Error> {
                co_await client->execSqlCoro(query, recipe_id, item, slot, count, input);
                co_return Error::Ok;
            });
        co_return res.value_or(err);
    }

    Task<Error> Database::addRecipeIngredientTag(const int64_t recipe_id, const std::string tag, const int slot, const int count,
                                                 const bool input) const {
        // language=postgresql
        static constexpr auto query = "INSERT INTO recipe_ingredient_tag (recipe_id, tag_id, slot, count, input) \
                                       SELECT $1 as recipe_id, id, $3 as slot, $4 as count, $5 as input \
                                       FROM tag \
                                       WHERE tag.loc = $2";
        const auto [res, err] =
            co_await handleDatabaseOperation<Error>([&, recipe_id, tag, slot, count, input](const DbClientPtr &client) -> Task<Error> {
                co_await client->execSqlCoro(query, recipe_id, tag, slot, count, input);
                co_return Error::Ok;
            });
        co_return res.value_or(err);
    }

    Task<std::vector<GlobalItem>> Database::getGlobalTagItems(int64_t tagId) const {
        // language=postgresql
        static constexpr auto query = "SELECT ver.id, ver.name, ver.project_id, item.loc \
                                       FROM tag_item_flat \
                                       JOIN project_item pitem ON pitem.id = child \
                                       JOIN item ON item.id = pitem.item_id \
                                       LEFT JOIN project_version ver ON ver.id = pitem.version_id \
                                       WHERE parent IN (SELECT ptag.id \
                                           FROM project_tag ptag \
                                           JOIN tag t on ptag.tag_id = t.id \
                                           WHERE t.id = $1)";
        const auto [res, err] =
            co_await handleDatabaseOperation<std::vector<GlobalItem>>([&, tagId](const DbClientPtr &client) -> Task<std::vector<GlobalItem>> {
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
        co_return res.value_or(std::vector<GlobalItem>{});
    }
}
